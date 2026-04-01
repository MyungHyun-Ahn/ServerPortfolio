#include "Pch.h"

#include "Foundation/Diagnostics/FCrashDump.h"
#include "Foundation/Logging/FCompositeLogger.h"
#include "Foundation/Logging/FConsoleLogger.h"
#include "Foundation/Logging/FFileLogger.h"
#include "Foundation/Logging/ILogger.h"
#include "Crypto/FDefaultPacketCipher.h"
#include "Servers/FServerFactory.h"
#include "Servers/IApplicationHandler.h"

#include <array>
#include <filesystem>
#include <Windows.h>


namespace
{
	constexpr std::uint8_t kPacketKey = 0x37;
	constexpr std::uint8_t kResponseRandomKey = 0x6C;

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
			GameServer::NetworkLib::Crypto::SDefaultPacketCipherConfig cipherConfig{};
			cipherConfig.packetKey = kPacketKey;
			m_packetCipher = std::make_shared<GameServer::NetworkLib::Crypto::FDefaultPacketCipher>(cipherConfig);
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

		void OnPacketReceived(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const char* buffer, std::int32_t length) override
		{
			if (length <= 1 || buffer == nullptr)
			{
				Log(GameServer::Foundation::ELogLevel::Warn, "received invalid encrypted packet.");
				return;
			}

			const std::uint8_t randomKey = static_cast<std::uint8_t>(buffer[0]);
			std::string message(buffer + 1, buffer + length);
			m_packetCipher->Decode(message.data(), static_cast<int>(message.size()), randomKey);
			std::ostringstream oss;
			oss << "received. sessionId=" << sessionId << " message=" << message;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());

			std::string responsePacket;
			responsePacket.resize(message.size() + 1);
			responsePacket[0] = static_cast<char>(kResponseRandomKey);
			std::copy(message.begin(), message.end(), responsePacket.begin() + 1);
			m_packetCipher->Encode(responsePacket.data() + 1, static_cast<int>(message.size()), kResponseRandomKey);
			server.Send(sessionId, responsePacket.data(), static_cast<std::int32_t>(responsePacket.size()));
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
		std::shared_ptr<GameServer::NetworkLib::Crypto::FDefaultPacketCipher> m_packetCipher;
	};
}

int main(int argc, char* argv[])
{
	GameServer::NetworkLib::SServerConfig serverConfig{};
	bool requestManualDump = false;
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

	compositeLogger->Log(GameServer::Foundation::ELogLevel::Info, "EchoServer", "Press Enter to stop server.");
	std::cin.get();
	server->Stop();
	GameServer::Foundation::FCrashDump::Shutdown();
	return 0;
}
