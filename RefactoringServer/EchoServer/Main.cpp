#include "Pch.h"

#include "Foundation/Diagnostics/FCrashDump.h"
#include "Foundation/Logging/FCompositeLogger.h"
#include "Foundation/Logging/FConsoleLogger.h"
#include "Foundation/Logging/FFileLogger.h"
#include "Foundation/Logging/ILogger.h"
#include "ContentsRuntime/Core/FContentRuntime.h"
#include "Crypto/FDefaultPacketCipher.h"
#include "EchoServer/Contents/Auth/FAuthContent.h"
#include "EchoServer/Contents/ContentTypes.h"
#include "EchoServer/Contents/Echo/FEchoContent.h"
#include "Generated/Packets/Chat/ChatPackets.h"
#include "Generated/Packets/Login/LoginPackets.h"
#include "Packet/Framing/FDefaultPacketFramer.h"
#include "Servers/Core/BackendTypes.h"
#include "Servers/Core/FServerFactory.h"
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

	const ContentsRuntime::Core::SContentRuntimeContentStats* FindContentStats(
		const ContentsRuntime::Core::SContentRuntimeStats& stats,
		const ContentsRuntime::Core::FContentId contentId) noexcept
	{
		for (const auto& contentStats : stats.contents)
		{
			if (contentStats.contentId == contentId)
			{
				return &contentStats;
			}
		}

		return nullptr;
	}

	std::uint64_t DeltaThreadCount(
		const ContentsRuntime::Core::SContentRuntimeContentStats* currentStats,
		const ContentsRuntime::Core::SContentRuntimeContentStats* previousStats,
		std::uint64_t ContentsRuntime::Core::SContentThreadStats::* member) noexcept
	{
		if (currentStats == nullptr)
		{
			return 0;
		}

		const std::uint64_t currentValue = currentStats->threadStats.*member;
		const std::uint64_t previousValue = previousStats != nullptr ? previousStats->threadStats.*member : 0;
		return currentValue >= previousValue ? (currentValue - previousValue) : 0;
	}

	double ToMicroseconds(std::uint64_t nanoseconds) noexcept
	{
		return static_cast<double>(nanoseconds) / 1000.0;
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

	class FEchoApplication final : public NetworkLib::IApplicationHandler
	{
	public:
		FEchoApplication(
			std::shared_ptr<Foundation::ILogger> logger,
			EchoServer::Contents::SRuntimeOptions runtimeOptions,
			ContentsRuntime::Core::SContentRuntimeConfig contentRuntimeConfig)
			: m_logger(std::move(logger))
			, m_runtimeOptions(runtimeOptions)
		{
			m_contentRuntime.SetConfig(contentRuntimeConfig);
			m_contentRuntime.RegisterContent(std::make_unique<EchoServer::Contents::FAuthContent>(m_logger));
			m_contentRuntime.RegisterContent(std::make_unique<EchoServer::Contents::FEchoContent>(m_logger, m_runtimeOptions));
		}

	public:
		void OnServerStarted(NetworkLib::IServer& server) override
		{
			m_contentRuntime.Start(server);

			std::ostringstream oss;
			oss << "EchoServer started. backend=" << static_cast<int>(server.GetBackendKind());
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		void OnClientConnected(std::uint64_t sessionId) override
		{
			m_contentRuntime.EnterSession(sessionId, EchoServer::Contents::kAuthContentId);

			std::ostringstream oss;
			oss << "client connected. sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		void OnPacketReceived(NetworkLib::IServer& server, std::uint64_t sessionId, const NetworkLib::Packet::View::FPacketView& packetView) override
		{
			(void)server;

			if (m_runtimeOptions.bootstrapTrace &&
				(packetView.opcode == Generated::Login::FLoginRq::kOpcode ||
				 packetView.opcode == Generated::Chat::FRoomSnapshotRq::kOpcode))
			{
				std::ostringstream oss;
				oss << "bootstrap trace: ingress packet. sessionId=" << sessionId
					<< " opcode=" << packetView.opcode
					<< " payloadBytes=" << packetView.payloadLength;
				Log(Foundation::ELogLevel::Info, oss.str());
			}

			if (!m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength) && m_runtimeOptions.logPackets)
			{
				std::ostringstream oss;
				oss << "Unhandled packet. sessionId=" << sessionId << " opcode=" << packetView.opcode;
				Log(Foundation::ELogLevel::Warn, oss.str());
			}
		}

		void OnClientDisconnected(std::uint64_t sessionId) override
		{
			m_contentRuntime.LeaveSession(sessionId);

			std::ostringstream oss;
			oss << "client disconnected. sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		void OnServerStopped() override
		{
			m_contentRuntime.Stop();
			Log(Foundation::ELogLevel::Info, "EchoServer stopped.");
		}

		ContentsRuntime::Core::SContentRuntimeStats GetContentStatsSnapshot()
		{
			return m_contentRuntime.GetStatsSnapshot();
		}

	private:
		void Log(Foundation::ELogLevel logLevel, const std::string& message) const
		{
			if (m_logger != nullptr)
			{
				m_logger->Log(logLevel, "EchoServer", message);
			}
		}

	private:
		std::shared_ptr<Foundation::ILogger> m_logger;
		EchoServer::Contents::SRuntimeOptions m_runtimeOptions;
		ContentsRuntime::Core::FContentRuntime m_contentRuntime;
	};
}

int main(int argc, char* argv[])
{
	NetworkLib::Core::SServerConfig serverConfig{};
	bool requestManualDump = false;
	bool runHeadless = false;
	EchoServer::Contents::SRuntimeOptions runtimeOptions{};
	ContentsRuntime::Core::SContentRuntimeConfig contentRuntimeConfig{};
	const std::filesystem::path executableDirectory = GetExecutableDirectory();
	serverConfig.backendKind = NetworkLib::Core::EBackendKind::Iocp;
	serverConfig.bindIp = "127.0.0.1";
	serverConfig.port = 19000;
	serverConfig.workerThreadCount = 2;
	serverConfig.maxSessionCount = 512;
	serverConfig.recvBufferSize = 1024;
	serverConfig.logConfig.minimumLevel = Foundation::ELogLevel::Info;
	serverConfig.logConfig.outputDirectory = (executableDirectory / "logs" / "EchoServer").string();
	serverConfig.logConfig.consoleEnabled = true;
	serverConfig.logConfig.fileEnabled = true;
	serverConfig.logConfig.includeThreadId = true;
	NetworkLib::Crypto::SDefaultPacketCipherConfig packetCipherConfig{};
	packetCipherConfig.packetKey = 0x37;
	serverConfig.packetCipher = std::make_shared<NetworkLib::Crypto::FDefaultPacketCipher>(packetCipherConfig);
	serverConfig.packetFramer = std::make_shared<NetworkLib::Packet::Framing::FDefaultPacketFramer>();

	if (argc >= 2)
	{
		for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
		{
			const std::string argument = argv[argumentIndex];
			if (argument == "rio")
			{
				serverConfig.backendKind = NetworkLib::Core::EBackendKind::Rio;
			}
			else if (argument == "asio")
			{
				serverConfig.backendKind = NetworkLib::Core::EBackendKind::BoostAsio;
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
			else if (argument == "--bootstrap-trace")
			{
				runtimeOptions.bootstrapTrace = true;
			}
			else if (argument == "--disable-page-pool")
			{
				runtimeOptions.enablePagePool = false;
			}
			else if (argument == "--page-size" && argumentIndex + 1 < argc)
			{
				runtimeOptions.pageSize = static_cast<std::uint32_t>(std::max(1, std::atoi(argv[++argumentIndex])));
			}
			else if (argument == "--contents-race-injection")
			{
				contentRuntimeConfig.enableRaceInjection = true;
			}
			else if (argument == "--contents-race-period" && argumentIndex + 1 < argc)
			{
				contentRuntimeConfig.raceInjectionPeriod =
					static_cast<std::uint32_t>(std::max(1, std::atoi(argv[++argumentIndex])));
			}
			else if (argument == "--contents-race-mode" && argumentIndex + 1 < argc)
			{
				const std::string mode = argv[++argumentIndex];
				if (mode == "switch")
				{
					contentRuntimeConfig.raceInjectionMode = ContentsRuntime::Core::ERaceInjectionMode::SwitchToThread;
				}
				else if (mode == "sleep0")
				{
					contentRuntimeConfig.raceInjectionMode = ContentsRuntime::Core::ERaceInjectionMode::Sleep0;
				}
				else if (mode == "yield")
				{
					contentRuntimeConfig.raceInjectionMode = ContentsRuntime::Core::ERaceInjectionMode::Yield;
				}
				else
				{
					contentRuntimeConfig.raceInjectionMode = ContentsRuntime::Core::ERaceInjectionMode::None;
				}
			}
			else if (argument == "--contents-fail-fast")
			{
				contentRuntimeConfig.failFastOnRuntimeError = true;
			}
		}
	}

	if (contentRuntimeConfig.enableRaceInjection && contentRuntimeConfig.raceInjectionPeriod == 0)
	{
		contentRuntimeConfig.raceInjectionPeriod = 100;
	}

	serverConfig.enablePageBufferReuse = runtimeOptions.enablePagePool;
	serverConfig.pageBufferSize = runtimeOptions.pageSize;

	auto compositeLogger = std::make_shared<Foundation::FCompositeLogger>();
	compositeLogger->AddSink(std::make_shared<Foundation::FConsoleLogger>(serverConfig.logConfig));
	compositeLogger->AddSink(std::make_shared<Foundation::FFileLogger>(serverConfig.logConfig));
	serverConfig.logger = compositeLogger;

	Foundation::SCrashDumpConfig crashDumpConfig{};
	crashDumpConfig.outputDirectory = (executableDirectory / "dumps" / "EchoServer").string();
	crashDumpConfig.logger = compositeLogger;
	Foundation::FCrashDump::Initialize(crashDumpConfig);

	if (requestManualDump)
	{
		const bool dumpWritten = Foundation::FCrashDump::WriteManualDumpForDiagnostics();
		Foundation::FCrashDump::Shutdown();
		return dumpWritten ? 0 : 1;
	}

	FEchoApplication echoApplication(compositeLogger, runtimeOptions, contentRuntimeConfig);
	std::unique_ptr<NetworkLib::IServer> server = NetworkLib::Core::FServerFactory::Create(serverConfig.backendKind);
	if (server == nullptr)
	{
		compositeLogger->Log(Foundation::ELogLevel::Error, "EchoServer", "server factory failed.");
		Foundation::FCrashDump::Shutdown();
		return 1;
	}

	if (!server->Start(serverConfig, echoApplication))
	{
		compositeLogger->Log(Foundation::ELogLevel::Error, "EchoServer", "server start failed.");
		Foundation::FCrashDump::Shutdown();
		return 1;
	}

	if (runHeadless)
	{
		compositeLogger->Log(Foundation::ELogLevel::Info, "EchoServer", "Headless mode enabled.");
		NetworkLib::Core::SServerStats previousStats = server->GetStatsSnapshot();
		ContentsRuntime::Core::SContentRuntimeStats previousContentStats = echoApplication.GetContentStatsSnapshot();
		SProcessMetricsSnapshot previousProcessMetrics = CaptureProcessMetricsSnapshot();
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			const NetworkLib::Core::SServerStats currentStats = server->GetStatsSnapshot();
			const ContentsRuntime::Core::SContentRuntimeStats currentContentStats = echoApplication.GetContentStatsSnapshot();
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
			const std::uint64_t moveTps =
				currentContentStats.moveSessionCount >= previousContentStats.moveSessionCount
					? currentContentStats.moveSessionCount - previousContentStats.moveSessionCount
					: 0;
			const std::uint64_t enqueueFailTps =
				currentContentStats.enqueueFailureCount >= previousContentStats.enqueueFailureCount
					? currentContentStats.enqueueFailureCount - previousContentStats.enqueueFailureCount
					: 0;
			const auto* currentAuthStats = FindContentStats(currentContentStats, EchoServer::Contents::kAuthContentId);
			const auto* previousAuthStats = FindContentStats(previousContentStats, EchoServer::Contents::kAuthContentId);
			const auto* currentEchoStats = FindContentStats(currentContentStats, EchoServer::Contents::kEchoContentId);
			const auto* previousEchoStats = FindContentStats(previousContentStats, EchoServer::Contents::kEchoContentId);
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
			std::cout
				<< "[ContentStats] contents=" << currentContentStats.registeredContentCount
				<< " sessions=" << currentContentStats.activeSessionCount
				<< " enterCalls=" << currentContentStats.enterSessionCallCount
				<< " leaveCalls=" << currentContentStats.leaveSessionCallCount
				<< " enqueueCalls=" << currentContentStats.enqueuePacketCallCount
				<< " moveTPS=" << moveTps
				<< " enqueueFailTPS=" << enqueueFailTps
				<< " runtimeEnqueueLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentContentStats.enqueuePacketLockWaitNs)
				<< " runtimeEnqueueMaxLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentContentStats.maxEnqueuePacketLockWaitNs)
				<< " runtimeMoveLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentContentStats.moveSessionLockWaitNs)
				<< " runtimeMoveMaxLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentContentStats.maxMoveSessionLockWaitNs)
				<< " authSessions=" << (currentAuthStats != nullptr ? currentAuthStats->activeSessionCount : 0)
				<< " authEnterTPS=" << DeltaThreadCount(currentAuthStats, previousAuthStats, &ContentsRuntime::Core::SContentThreadStats::enterCount)
				<< " authLeaveTPS=" << DeltaThreadCount(currentAuthStats, previousAuthStats, &ContentsRuntime::Core::SContentThreadStats::leaveCount)
				<< " authPacketTPS=" << DeltaThreadCount(currentAuthStats, previousAuthStats, &ContentsRuntime::Core::SContentThreadStats::packetCount)
				<< " authQueue=" << (currentAuthStats != nullptr ? currentAuthStats->threadStats.packetQueueDepth : 0)
				<< " authMaxQueue=" << (currentAuthStats != nullptr ? currentAuthStats->threadStats.maxPacketQueueDepth : 0)
				<< " authPacketEnqueueCalls=" << (currentAuthStats != nullptr ? currentAuthStats->threadStats.enqueuePacketCallCount : 0)
				<< " authPacketEnqueueLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentAuthStats != nullptr ? currentAuthStats->threadStats.enqueuePacketLockWaitNs : 0)
				<< " authPacketEnqueueMaxLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentAuthStats != nullptr ? currentAuthStats->threadStats.maxEnqueuePacketLockWaitNs : 0)
				<< " echoSessions=" << (currentEchoStats != nullptr ? currentEchoStats->activeSessionCount : 0)
				<< " echoEnterTPS=" << DeltaThreadCount(currentEchoStats, previousEchoStats, &ContentsRuntime::Core::SContentThreadStats::enterCount)
				<< " echoLeaveTPS=" << DeltaThreadCount(currentEchoStats, previousEchoStats, &ContentsRuntime::Core::SContentThreadStats::leaveCount)
				<< " echoPacketTPS=" << DeltaThreadCount(currentEchoStats, previousEchoStats, &ContentsRuntime::Core::SContentThreadStats::packetCount)
				<< " echoFrameTPS=" << DeltaThreadCount(currentEchoStats, previousEchoStats, &ContentsRuntime::Core::SContentThreadStats::frameCount)
				<< " echoQueue=" << (currentEchoStats != nullptr ? currentEchoStats->threadStats.packetQueueDepth : 0)
				<< " echoMaxQueue=" << (currentEchoStats != nullptr ? currentEchoStats->threadStats.maxPacketQueueDepth : 0)
				<< " echoPacketEnqueueCalls=" << (currentEchoStats != nullptr ? currentEchoStats->threadStats.enqueuePacketCallCount : 0)
				<< " echoPacketEnqueueLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentEchoStats != nullptr ? currentEchoStats->threadStats.enqueuePacketLockWaitNs : 0)
				<< " echoPacketEnqueueMaxLockUs=" << std::fixed << std::setprecision(2) << ToMicroseconds(currentEchoStats != nullptr ? currentEchoStats->threadStats.maxEnqueuePacketLockWaitNs : 0)
				<< " echoLastDelayFrame=" << (currentEchoStats != nullptr ? currentEchoStats->threadStats.lastDelayFrame : 0)
				<< " echoMaxDelayFrame=" << (currentEchoStats != nullptr ? currentEchoStats->threadStats.maxDelayFrame : 0)
				<< std::endl;
			previousStats = currentStats;
			previousContentStats = currentContentStats;
			previousProcessMetrics = currentProcessMetrics;
		}
	}

	compositeLogger->Log(Foundation::ELogLevel::Info, "EchoServer", "Press Enter to stop server.");
	std::cin.get();
	server->Stop();
	Foundation::FCrashDump::Shutdown();
	return 0;
}
