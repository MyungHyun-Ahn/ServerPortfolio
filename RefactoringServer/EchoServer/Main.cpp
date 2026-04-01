#include "Pch.h"

#include "Foundation/Diagnostics/FCrashDump.h"
#include "Foundation/Logging/FCompositeLogger.h"
#include "Foundation/Logging/FConsoleLogger.h"
#include "Foundation/Logging/FFileLogger.h"
#include "Foundation/Logging/ILogger.h"
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
#include <mutex>
#include <unordered_map>
#include <thread>
#include <Windows.h>


namespace
{
	struct SServerRuntimeOptions
	{
		int sendThreadCount = 1;
		int responsesPerThread = 1;
		bool logPackets = false;
	};

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
		, public GameServer::Generated::Echo::FEchoPacketHandlerBase
		, public GameServer::Generated::Login::FLoginPacketHandlerBase
	{
	public:
		FEchoApplication(
			std::shared_ptr<GameServer::Foundation::ILogger> logger,
			SServerRuntimeOptions runtimeOptions)
			: m_logger(std::move(logger))
			, m_runtimeOptions(runtimeOptions)
		{
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
			{
				std::lock_guard<std::mutex> lock(m_loginMutex);
				m_loggedInUsers.erase(sessionId);
			}

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
				oss << "received. sessionId=" << sessionId << " opcode=" << packet.GetOpcode() << " message=" << packet.message;
				Log(GameServer::Foundation::ELogLevel::Info, oss.str());
			}

			if (m_runtimeOptions.sendThreadCount == 1 && m_runtimeOptions.responsesPerThread == 1)
			{
				GameServer::Generated::Echo::FEchoRp responsePacket;
				responsePacket.message = packet.message;
				return GameServer::Generated::Echo::SendGeneratedPacket(server, sessionId, responsePacket);
			}

			std::vector<std::thread> sendThreads;
			sendThreads.reserve(static_cast<std::size_t>(m_runtimeOptions.sendThreadCount));
			for (int threadIndex = 0; threadIndex < m_runtimeOptions.sendThreadCount; ++threadIndex)
			{
				sendThreads.emplace_back([&, threadIndex, sessionId, message = packet.message]()
				{
					for (int responseIndex = 0; responseIndex < m_runtimeOptions.responsesPerThread; ++responseIndex)
					{
						std::ostringstream responseBuilder;
						responseBuilder << message
							<< "|t=" << threadIndex
							<< "|r=" << responseIndex;

						GameServer::Generated::Echo::FEchoRp responsePacket;
						responsePacket.message = responseBuilder.str();
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

		bool HandleLoginRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::Generated::Login::FLoginRq& packet) override
		{
			const bool success = packet.userId != 0;
			{
				std::lock_guard<std::mutex> lock(m_loginMutex);
				if (success)
				{
					m_loggedInUsers[sessionId] = packet.userId;
				}
				else
				{
					m_loggedInUsers.erase(sessionId);
				}
			}

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
		bool IsLoggedIn(std::uint64_t sessionId)
		{
			std::lock_guard<std::mutex> lock(m_loginMutex);
			return m_loggedInUsers.contains(sessionId);
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
		std::mutex m_loginMutex;
		std::unordered_map<std::uint64_t, std::uint32_t> m_loggedInUsers;
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
		}
	}

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

	FEchoApplication echoApplication(compositeLogger, runtimeOptions);
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
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			const GameServer::NetworkLib::SServerStats currentStats = server->GetStatsSnapshot();
			const std::uint64_t acceptTps = currentStats.acceptedSessionCount - previousStats.acceptedSessionCount;
			const std::uint64_t recvTps = currentStats.receivedPacketCount - previousStats.receivedPacketCount;
			const std::uint64_t sendTps = currentStats.sentPacketCount - previousStats.sentPacketCount;
			const std::uint64_t wsaRecvTps = currentStats.wsaRecvCallCount - previousStats.wsaRecvCallCount;
			const std::uint64_t wsaSendTps = currentStats.wsaSendCallCount - previousStats.wsaSendCallCount;
			std::cout
				<< "[EchoStats] sessions=" << currentStats.activeSessionCount
				<< " acceptTPS=" << acceptTps
				<< " recvTPS=" << recvTps
				<< " sendTPS=" << sendTps
				<< " wsaSendTPS=" << wsaSendTps
				<< " wsaRecvTPS=" << wsaRecvTps
				<< " totalWSASendCalls=" << currentStats.wsaSendCallCount
				<< " totalWSARecvCalls=" << currentStats.wsaRecvCallCount
				<< std::endl;
			previousStats = currentStats;
		}
	}

	compositeLogger->Log(GameServer::Foundation::ELogLevel::Info, "EchoServer", "Press Enter to stop server.");
	std::cin.get();
	server->Stop();
	GameServer::Foundation::FCrashDump::Shutdown();
	return 0;
}
