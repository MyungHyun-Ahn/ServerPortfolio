#include "Pch.h"

#include "Foundation/Diagnostics/FCrashDump.h"
#include "Foundation/Logging/FCompositeLogger.h"
#include "Foundation/Logging/FConsoleLogger.h"
#include "Foundation/Logging/FFileLogger.h"
#include "Foundation/Logging/ILogger.h"
#include "Crypto/FDefaultPacketCipher.h"
#include "Packet/FDefaultPacketFramer.h"
#include "Servers/FServerFactory.h"
#include "Servers/IApplicationHandler.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <thread>
#include <Windows.h>


namespace
{
	constexpr std::uint16_t kEchoRequestOpcode = 1000;
	constexpr std::uint16_t kEchoResponseOpcode = 1001;

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

	class FEchoApplication final : public GameServer::NetworkLib::IApplicationHandler
	{
	public:
		explicit FEchoApplication(std::shared_ptr<GameServer::Foundation::ILogger> logger)
			: m_logger(std::move(logger))
		{
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

		void OnPacketReceived(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) override
		{
			std::string message(buffer, buffer + length);
			std::ostringstream oss;
			oss << "received. sessionId=" << sessionId << " opcode=" << opcode << " message=" << message;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());

			if (opcode == kEchoRequestOpcode)
			{
				server.Send(sessionId, kEchoResponseOpcode, buffer, length);
			}
		}

		void OnClientDisconnected(std::uint64_t sessionId) override
		{
			std::ostringstream oss;
			oss << "client disconnected. sessionId=" << sessionId;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}

		void OnServerStopped() override
		{
			Log(GameServer::Foundation::ELogLevel::Info, "EchoServer stopped.");
		}

	private:
		void Log(GameServer::Foundation::ELogLevel logLevel, const std::string& message) const
		{
			if (m_logger != nullptr)
			{
				m_logger->Log(logLevel, "EchoServer", message);
			}
		}

	private:
		std::shared_ptr<GameServer::Foundation::ILogger> m_logger;
	};
}

int main(int argc, char* argv[])
{
	GameServer::NetworkLib::SServerConfig serverConfig{};
	bool requestManualDump = false;
	bool runHeadless = false;
	const std::filesystem::path executableDirectory = GetExecutableDirectory();
	serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::Iocp;
	serverConfig.bindIp = "127.0.0.1";
	serverConfig.port = 19000;
	serverConfig.workerThreadCount = 2;
	serverConfig.maxSessionCount = 64;
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

	FEchoApplication echoApplication(compositeLogger);
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
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	compositeLogger->Log(GameServer::Foundation::ELogLevel::Info, "EchoServer", "Press Enter to stop server.");
	std::cin.get();
	server->Stop();
	GameServer::Foundation::FCrashDump::Shutdown();
	return 0;
}
