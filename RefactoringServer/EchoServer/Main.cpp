#include "Pch.h"

#include "Servers/FServerFactory.h"
#include "Servers/IApplicationHandler.h"
#include "Logging/ILogger.h"
#include "Logging/FCompositeLogger.h"
#include "Logging/FConsoleLogger.h"
#include "Logging/FFileLogger.h"


namespace
{
	class FEchoApplication final : public GameServer::NetworkLib::IApplicationHandler
	{
	public:
		explicit FEchoApplication(std::shared_ptr<GameServer::NetworkLib::ILogger> logger)
			: m_logger(std::move(logger))
		{
		}

	public:
		void OnServerStarted(GameServer::NetworkLib::IServer& server) override
		{
			std::ostringstream oss;
			oss << "EchoServer started. backend=" << static_cast<int>(server.GetBackendKind());
			Log(GameServer::NetworkLib::ELogLevel::Info, oss.str());
		}

		void OnClientConnected(std::uint64_t sessionId) override
		{
			std::ostringstream oss;
			oss << "client connected. sessionId=" << sessionId;
			Log(GameServer::NetworkLib::ELogLevel::Info, oss.str());
		}

		void OnPacketReceived(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const char* buffer, std::int32_t length) override
		{
			std::string message(buffer, buffer + length);
			std::ostringstream oss;
			oss << "received. sessionId=" << sessionId << " message=" << message;
			Log(GameServer::NetworkLib::ELogLevel::Info, oss.str());
			server.Send(sessionId, buffer, length);
		}

		void OnClientDisconnected(std::uint64_t sessionId) override
		{
			std::ostringstream oss;
			oss << "client disconnected. sessionId=" << sessionId;
			Log(GameServer::NetworkLib::ELogLevel::Info, oss.str());
		}

		void OnServerStopped() override
		{
			Log(GameServer::NetworkLib::ELogLevel::Info, "EchoServer stopped.");
		}

	private:
		void Log(GameServer::NetworkLib::ELogLevel logLevel, const std::string& message) const
		{
			if (m_logger != nullptr)
			{
				m_logger->Log(logLevel, "EchoServer", message);
			}
		}

	private:
		std::shared_ptr<GameServer::NetworkLib::ILogger> m_logger;
	};
}

int main(int argc, char* argv[])
{
	GameServer::NetworkLib::SServerConfig serverConfig{};
	serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::Iocp;
	serverConfig.bindIp = "127.0.0.1";
	serverConfig.port = 19000;
	serverConfig.workerThreadCount = 2;
	serverConfig.maxSessionCount = 64;
	serverConfig.recvBufferSize = 1024;
	serverConfig.logConfig.minimumLevel = GameServer::NetworkLib::ELogLevel::Info;
	serverConfig.logConfig.outputDirectory = "logs";
	serverConfig.logConfig.consoleEnabled = true;
	serverConfig.logConfig.fileEnabled = true;
	serverConfig.logConfig.includeThreadId = true;

	if (argc >= 2)
	{
		const std::string backendName = argv[1];
		if (backendName == "rio")
		{
			serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::Rio;
		}
		else if (backendName == "asio")
		{
			serverConfig.backendKind = GameServer::NetworkLib::EBackendKind::BoostAsio;
		}
	}

	auto compositeLogger = std::make_shared<GameServer::NetworkLib::FCompositeLogger>();
	compositeLogger->AddSink(std::make_shared<GameServer::NetworkLib::FConsoleLogger>(serverConfig.logConfig));
	compositeLogger->AddSink(std::make_shared<GameServer::NetworkLib::FFileLogger>(serverConfig.logConfig));
	serverConfig.logger = compositeLogger;

	FEchoApplication echoApplication(compositeLogger);
	std::unique_ptr<GameServer::NetworkLib::IServer> server = GameServer::NetworkLib::FServerFactory::Create(serverConfig.backendKind);
	if (server == nullptr)
	{
		compositeLogger->Log(GameServer::NetworkLib::ELogLevel::Error, "EchoServer", "server factory failed.");
		return 1;
	}

	if (!server->Start(serverConfig, echoApplication))
	{
		compositeLogger->Log(GameServer::NetworkLib::ELogLevel::Error, "EchoServer", "server start failed.");
		return 1;
	}

	compositeLogger->Log(GameServer::NetworkLib::ELogLevel::Info, "EchoServer", "Press Enter to stop server.");
	std::cin.get();
	server->Stop();
	return 0;
}
