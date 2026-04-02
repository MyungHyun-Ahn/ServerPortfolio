#include "Pch.h"

#include "Foundation/Diagnostics/FCrashDump.h"
#include "Foundation/Logging/FCompositeLogger.h"
#include "Foundation/Logging/FConsoleLogger.h"
#include "Foundation/Logging/FFileLogger.h"
#include "Foundation/Logging/ILogger.h"
#include "Generated/Packets/Chat/ChatPacketHandler.h"
#include "Generated/Packets/Echo/EchoPacketHandler.h"
#include "Generated/Packets/Login/LoginPacketHandler.h"
#include "Generated/Packets/PacketRouter.h"
#include "Crypto/FDefaultPacketCipher.h"
#include "Packet/FDefaultPacketFramer.h"
#include "Servers/FServerFactory.h"
#include "Servers/IApplicationHandler.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <Psapi.h>
#include <thread>
#include <Windows.h>

#pragma comment(lib, "Psapi.lib")

namespace
{
	struct SServerRuntimeOptions
	{
		int sendThreadCount = 1;
		int responsesPerThread = 1;
		bool logPackets = false;
		bool enablePagePool = true;
		std::uint32_t pageSize = 4096;
	};

	struct SProcessMetricsSnapshot
	{
		ULONGLONG tickCountMs = 0;
		std::uint64_t processTime100ns = 0;
		SIZE_T workingSetBytes = 0;
		SIZE_T peakWorkingSetBytes = 0;
		bool valid = false;
	};

	std::uint64_t FileTimeToUInt64(const FILETIME& fileTime) noexcept
	{
		ULARGE_INTEGER value{};
		value.LowPart = fileTime.dwLowDateTime;
		value.HighPart = fileTime.dwHighDateTime;
		return value.QuadPart;
	}

	SProcessMetricsSnapshot CaptureProcessMetricsSnapshot() noexcept
	{
		SProcessMetricsSnapshot snapshot{};
		snapshot.tickCountMs = GetTickCount64();

		FILETIME creationTime{};
		FILETIME exitTime{};
		FILETIME kernelTime{};
		FILETIME userTime{};
		if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
		{
			return snapshot;
		}

		PROCESS_MEMORY_COUNTERS_EX memoryCounters{};
		if (!GetProcessMemoryInfo(
			GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters),
			sizeof(memoryCounters)))
		{
			return snapshot;
		}

		snapshot.processTime100ns = FileTimeToUInt64(kernelTime) + FileTimeToUInt64(userTime);
		snapshot.workingSetBytes = memoryCounters.WorkingSetSize;
		snapshot.peakWorkingSetBytes = memoryCounters.PeakWorkingSetSize;
		snapshot.valid = true;
		return snapshot;
	}

	double CalculateCpuUsagePercent(const SProcessMetricsSnapshot& previous, const SProcessMetricsSnapshot& current) noexcept
	{
		if (!previous.valid || !current.valid || current.tickCountMs <= previous.tickCountMs)
		{
			return 0.0;
		}

		const std::uint64_t wallTime100ns =
			static_cast<std::uint64_t>(current.tickCountMs - previous.tickCountMs) * 10000ULL;
		if (wallTime100ns == 0 || current.processTime100ns < previous.processTime100ns)
		{
			return 0.0;
		}

		SYSTEM_INFO systemInfo{};
		GetSystemInfo(&systemInfo);
		const std::uint32_t logicalProcessorCount =
			std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(systemInfo.dwNumberOfProcessors));
		const double processTimeDelta = static_cast<double>(current.processTime100ns - previous.processTime100ns);
		const double totalTime = static_cast<double>(wallTime100ns) * static_cast<double>(logicalProcessorCount);
		if (totalTime <= 0.0)
		{
			return 0.0;
		}

		return (processTimeDelta / totalTime) * 100.0;
	}

	double BytesToMegabytes(const SIZE_T bytes) noexcept
	{
		return static_cast<double>(bytes) / (1024.0 * 1024.0);
	}

	std::filesystem::path GetExecutableDirectory()
	{
		std::array<char, MAX_PATH> modulePath = {};
		const DWORD pathLength = GetModuleFileNameA(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
		if (pathLength == 0 || pathLength >= modulePath.size())
		{
			return std::filesystem::current_path();
		}

		return std::filesystem::path(modulePath.data()).parent_path();
	}

	class FEchoApplication final
		: public GameServer::NetworkLib::IApplicationHandler
		, public GameServer::Generated::Chat::FChatPacketHandlerBase
		, public GameServer::Generated::Echo::FEchoPacketHandlerBase
		, public GameServer::Generated::Login::FLoginPacketHandlerBase
	{
	public:
		FEchoApplication(
			std::shared_ptr<GameServer::Foundation::ILogger> logger,
			std::uint32_t maxSessionCount,
			SServerRuntimeOptions runtimeOptions)
			: m_logger(std::move(logger))
			, m_runtimeOptions(runtimeOptions)
			, m_loggedInUsers(static_cast<std::size_t>(maxSessionCount))
		{
			m_packetRouter.SetChatHandler(this);
			m_packetRouter.SetEchoHandler(this);
			m_packetRouter.SetLoginHandler(this);
		}

	public:
		void OnServerStarted(GameServer::NetworkLib::IServer& server) override
		{
			std::ostringstream oss;
			oss << "EchoServer started. backend=" << static_cast<int>(server.GetBackendKind());
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}

		void OnClientConnected(std::uint64_t sessionId) override
		{
			std::ostringstream oss;
			oss << "client connected. sessionId=" << sessionId;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}

		void OnPacketReceived(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) override
		{
			if (!m_packetRouter.DispatchPacket(server, sessionId, packetView) && m_runtimeOptions.logPackets)
			{
				std::ostringstream oss;
				oss << "Unhandled packet. sessionId=" << sessionId << " opcode=" << packetView.opcode;
				Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
			}
		}

		void OnClientDisconnected(std::uint64_t sessionId) override
		{
			ResetLoggedInUser(sessionId);

			std::ostringstream oss;
			oss << "client disconnected. sessionId=" << sessionId;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}

		void OnServerStopped() override
		{
			Log(GameServer::Foundation::ELogLevel::Info, "EchoServer stopped.");
		}

		bool HandleEchoRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::Generated::Echo::FEchoRq& packet) override
		{
			if (!IsLoggedIn(sessionId))
			{
				Log(GameServer::Foundation::ELogLevel::Warn, "echo request rejected before login.");
				return false;
			}

			if (m_runtimeOptions.logPackets)
			{
				std::ostringstream oss;
				oss << "received. sessionId=" << sessionId << " opcode=" << packet.GetOpcode() << " message=" << packet.GetMessageValue();
				Log(GameServer::Foundation::ELogLevel::Info, oss.str());
			}

			if (m_runtimeOptions.sendThreadCount == 1 && m_runtimeOptions.responsesPerThread == 1)
			{
				GameServer::Generated::Echo::FEchoRp responsePacket;
				responsePacket.SetMessageValue(packet.GetMessageValue());
				return GameServer::Generated::Echo::SendGeneratedPacket(server, sessionId, responsePacket);
			}

			std::vector<std::thread> sendThreads;
			sendThreads.reserve(static_cast<std::size_t>(m_runtimeOptions.sendThreadCount));
			for (int threadIndex = 0; threadIndex < m_runtimeOptions.sendThreadCount; ++threadIndex)
			{
				sendThreads.emplace_back([&, threadIndex, sessionId, message = std::string(packet.GetMessageValue())]()
				{
					for (int responseIndex = 0; responseIndex < m_runtimeOptions.responsesPerThread; ++responseIndex)
					{
						std::ostringstream responseBuilder;
						responseBuilder << message
							<< "|t=" << threadIndex
							<< "|r=" << responseIndex;

						const std::string responseMessage = responseBuilder.str();
						GameServer::Generated::Echo::FEchoRp responsePacket;
						responsePacket.SetMessageValue(responseMessage);
						GameServer::Generated::Echo::SendGeneratedPacket(server, sessionId, responsePacket);
					}
				});
			}

			for (std::thread& sendThread : sendThreads)
			{
				sendThread.join();
			}

			return true;
		}

		bool HandleRoomSnapshotRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::Generated::Chat::FRoomSnapshotRq& packet) override
		{
			if (!IsLoggedIn(sessionId))
			{
				Log(GameServer::Foundation::ELogLevel::Warn, "chat snapshot request rejected before login.");
				return false;
			}

			GameServer::Generated::Chat::FRoomSnapshotRp snapshotPacket;
			snapshotPacket.roomId = packet.roomId;
			snapshotPacket.participants = { "alpha", "bravo", "charlie" };
			snapshotPacket.unreadCounts = {
				{ "alpha", 1u },
				{ "bravo", 3u },
				{ "charlie", 5u }
			};
			snapshotPacket.metadata = {
				{ "topic", "general" },
				{ "owner", "alpha" }
			};

			const bool snapshotSent = GameServer::Generated::Chat::SendGeneratedPacket(server, sessionId, snapshotPacket);

			const std::array<std::uint8_t, 8> binaryPayload = {
				static_cast<std::uint8_t>(packet.roomId & 0xFF),
				static_cast<std::uint8_t>((packet.roomId >> 8) & 0xFF),
				0x10, 0x20, 0x30, 0x40, 0x50, 0x60
			};

			GameServer::Generated::Chat::FRoomBinarySnapshotNoti binarySnapshotPacket;
			binarySnapshotPacket.roomId = packet.roomId;
			binarySnapshotPacket.SetPayloadValue(std::span<const std::uint8_t>(binaryPayload.data(), binaryPayload.size()));
			const bool binarySnapshotSent = GameServer::Generated::Chat::SendGeneratedPacket(server, sessionId, binarySnapshotPacket);

			if (m_runtimeOptions.logPackets)
			{
				std::ostringstream oss;
				oss << "chat snapshot served. sessionId=" << sessionId
					<< " roomId=" << packet.roomId
					<< " binaryBytes=" << binaryPayload.size();
				Log(GameServer::Foundation::ELogLevel::Info, oss.str());
			}

			return snapshotSent && binarySnapshotSent;
		}

		bool HandleLoginRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::Generated::Login::FLoginRq& packet) override
		{
			const bool success = packet.userId != 0;
			SetLoggedInUser(sessionId, success ? packet.userId : 0);

			{
				std::ostringstream oss;
				oss << "login " << (success ? "succeeded" : "failed")
					<< ". sessionId=" << sessionId
					<< " userId=" << packet.userId;
				Log(success ? GameServer::Foundation::ELogLevel::Info : GameServer::Foundation::ELogLevel::Warn, oss.str());
			}

			GameServer::Generated::Login::FLoginRp responsePacket;
			responsePacket.userId = packet.userId;
			responsePacket.success = success;
			return GameServer::Generated::Login::SendGeneratedPacket(server, sessionId, responsePacket);
		}

	private:
		std::uint32_t GetSessionSlotIndex(std::uint64_t sessionId) const noexcept
		{
			return static_cast<std::uint32_t>(sessionId & 0xFFFFFFFFULL);
		}

		bool IsLoggedIn(std::uint64_t sessionId)
		{
			const std::uint32_t slotIndex = GetSessionSlotIndex(sessionId);
			return slotIndex < m_loggedInUsers.size() &&
				m_loggedInUsers[slotIndex].load(std::memory_order_relaxed) != 0;
		}

		void SetLoggedInUser(std::uint64_t sessionId, std::uint32_t userId) noexcept
		{
			const std::uint32_t slotIndex = GetSessionSlotIndex(sessionId);
			if (slotIndex < m_loggedInUsers.size())
			{
				m_loggedInUsers[slotIndex].store(userId, std::memory_order_relaxed);
			}
		}

		void ResetLoggedInUser(std::uint64_t sessionId) noexcept
		{
			SetLoggedInUser(sessionId, 0);
		}

		void Log(GameServer::Foundation::ELogLevel logLevel, const std::string& message) const
		{
			if (m_logger != nullptr)
			{
				m_logger->Log(logLevel, "EchoServer", message);
			}
		}

	private:
		std::shared_ptr<GameServer::Foundation::ILogger> m_logger;
		SServerRuntimeOptions m_runtimeOptions;
		GameServer::Generated::FPacketRouter m_packetRouter;
		std::vector<std::atomic<std::uint32_t>> m_loggedInUsers;
	};
}

int main(int argc, char* argv[])
{
	GameServer::NetworkLib::SServerConfig serverConfig{};
	bool requestManualDump = false;
	bool runHeadless = false;
	SServerRuntimeOptions runtimeOptions{};
	const std::filesystem::path executableDirectory = GetExecutableDirectory();
	serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::Iocp;
	serverConfig.bindIp = "127.0.0.1";
	serverConfig.port = 19000;
	serverConfig.workerThreadCount = 2;
	serverConfig.maxSessionCount = 512;
	serverConfig.recvBufferSize = 1024;
	serverConfig.logConfig.minimumLevel = GameServer::Foundation::ELogLevel::Info;
	serverConfig.logConfig.outputDirectory = (executableDirectory / "logs" / "EchoServer").string();
	serverConfig.logConfig.consoleEnabled = true;
	serverConfig.logConfig.fileEnabled = true;
	serverConfig.logConfig.includeThreadId = true;
	GameServer::NetworkLib::Crypto::SDefaultPacketCipherConfig packetCipherConfig{};
	packetCipherConfig.packetKey = 0x37;
	serverConfig.packetCipher = std::make_shared<GameServer::NetworkLib::Crypto::FDefaultPacketCipher>(packetCipherConfig);
	serverConfig.packetFramer = std::make_shared<GameServer::NetworkLib::Packet::FDefaultPacketFramer>();

	if (argc >= 2)
	{
		for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
		{
			const std::string argument = argv[argumentIndex];
			if (argument == "rio")
			{
				serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::Rio;
			}
			else if (argument == "asio")
			{
				serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::BoostAsio;
			}
			else if (argument == "--manual-dump")
			{
				requestManualDump = true;
			}
			else if (argument == "--headless")
			{
				runHeadless = true;
			}
			else if (argument == "--send-thread-count" && argumentIndex + 1 < argc)
			{
				runtimeOptions.sendThreadCount = std::max(1, std::atoi(argv[++argumentIndex]));
			}
			else if (argument == "--responses-per-thread" && argumentIndex + 1 < argc)
			{
				runtimeOptions.responsesPerThread = std::max(1, std::atoi(argv[++argumentIndex]));
			}
			else if (argument == "--log-packets")
			{
				runtimeOptions.logPackets = true;
			}
			else if (argument == "--disable-page-pool")
			{
				runtimeOptions.enablePagePool = false;
			}
			else if (argument == "--page-size" && argumentIndex + 1 < argc)
			{
				runtimeOptions.pageSize = static_cast<std::uint32_t>(std::max(1, std::atoi(argv[++argumentIndex])));
			}
		}
	}

	serverConfig.enablePageBufferReuse = runtimeOptions.enablePagePool;
	serverConfig.pageBufferSize = runtimeOptions.pageSize;

	auto compositeLogger = std::make_shared<GameServer::Foundation::FCompositeLogger>();
	compositeLogger->AddSink(std::make_shared<GameServer::Foundation::FConsoleLogger>(serverConfig.logConfig));
	compositeLogger->AddSink(std::make_shared<GameServer::Foundation::FFileLogger>(serverConfig.logConfig));
	serverConfig.logger = compositeLogger;

	GameServer::Foundation::SCrashDumpConfig crashDumpConfig{};
	crashDumpConfig.outputDirectory = (executableDirectory / "dumps" / "EchoServer").string();
	crashDumpConfig.logger = compositeLogger;
	GameServer::Foundation::FCrashDump::Initialize(crashDumpConfig);

	if (requestManualDump)
	{
		const bool dumpWritten = GameServer::Foundation::FCrashDump::WriteManualDumpForDiagnostics();
		GameServer::Foundation::FCrashDump::Shutdown();
		return dumpWritten ? 0 : 1;
	}

	FEchoApplication echoApplication(compositeLogger, serverConfig.maxSessionCount, runtimeOptions);
	std::unique_ptr<GameServer::NetworkLib::IServer> server = GameServer::NetworkLib::FServerFactory::Create(serverConfig.backendKind);
	if (server == nullptr)
	{
		compositeLogger->Log(GameServer::Foundation::ELogLevel::Error, "EchoServer", "server factory failed.");
		GameServer::Foundation::FCrashDump::Shutdown();
		return 1;
	}

	if (!server->Start(serverConfig, echoApplication))
	{
		compositeLogger->Log(GameServer::Foundation::ELogLevel::Error, "EchoServer", "server start failed.");
		GameServer::Foundation::FCrashDump::Shutdown();
		return 1;
	}

	if (runHeadless)
	{
		compositeLogger->Log(GameServer::Foundation::ELogLevel::Info, "EchoServer", "Headless mode enabled.");
		GameServer::NetworkLib::SServerStats previousStats = server->GetStatsSnapshot();
		SProcessMetricsSnapshot previousProcessMetrics = CaptureProcessMetricsSnapshot();
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			const GameServer::NetworkLib::SServerStats currentStats = server->GetStatsSnapshot();
			const SProcessMetricsSnapshot currentProcessMetrics = CaptureProcessMetricsSnapshot();
			const std::uint64_t acceptTps = currentStats.acceptedSessionCount - previousStats.acceptedSessionCount;
			const std::uint64_t recvTps = currentStats.receivedPacketCount - previousStats.receivedPacketCount;
			const std::uint64_t sendTps = currentStats.sentPacketCount - previousStats.sentPacketCount;
			const std::uint64_t recvBytesPerSec = currentStats.receivedByteCount - previousStats.receivedByteCount;
			const std::uint64_t sendBytesPerSec = currentStats.sentByteCount - previousStats.sentByteCount;
			const std::uint64_t wsaRecvTps = currentStats.wsaRecvCallCount - previousStats.wsaRecvCallCount;
			const std::uint64_t wsaSendTps = currentStats.wsaSendCallCount - previousStats.wsaSendCallCount;
			const double cpuUsagePercent = CalculateCpuUsagePercent(previousProcessMetrics, currentProcessMetrics);
			const double workingSetMb = currentProcessMetrics.valid ? BytesToMegabytes(currentProcessMetrics.workingSetBytes) : 0.0;
			const double peakWorkingSetMb = currentProcessMetrics.valid ? BytesToMegabytes(currentProcessMetrics.peakWorkingSetBytes) : 0.0;
			std::cout
				<< "[EchoStats] sessions=" << currentStats.activeSessionCount
				<< " acceptTPS=" << acceptTps
				<< " recvTPS=" << recvTps
				<< " sendTPS=" << sendTps
				<< " recvBps=" << recvBytesPerSec
				<< " sendBps=" << sendBytesPerSec
				<< " wsaSendTPS=" << wsaSendTps
				<< " wsaRecvTPS=" << wsaRecvTps
				<< " queuedSendBuffers=" << currentStats.queuedSendBufferCount
				<< " maxQueuedSendBuffers=" << currentStats.maxObservedQueuedSendBufferCount
				<< " sessionPool=" << currentStats.sessionPoolUsage << "/" << currentStats.sessionPoolCapacity
				<< " sendBufferPool=" << currentStats.sendBufferPoolUsage << "/" << currentStats.sendBufferPoolCapacity
				<< " packetBufferPool=" << currentStats.packetBufferPoolUsage << "/" << currentStats.packetBufferPoolCapacity
				<< " cpuPercent=" << std::fixed << std::setprecision(2) << cpuUsagePercent
				<< " workingSetMB=" << std::fixed << std::setprecision(2) << workingSetMb
				<< " peakWorkingSetMB=" << std::fixed << std::setprecision(2) << peakWorkingSetMb
				<< " totalWSASendCalls=" << currentStats.wsaSendCallCount
				<< " totalWSARecvCalls=" << currentStats.wsaRecvCallCount
				<< std::endl;
			previousStats = currentStats;
			previousProcessMetrics = currentProcessMetrics;
		}
	}

	compositeLogger->Log(GameServer::Foundation::ELogLevel::Info, "EchoServer", "Press Enter to stop server.");
	std::cin.get();
	server->Stop();
	GameServer::Foundation::FCrashDump::Shutdown();
	return 0;
}
