#include "NetworkLib/ServerFactory.h"
#include "NetworkLib/IApplicationHandler.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{
	class FEchoApplication final : public GameServer::NetworkLib::IApplicationHandler
	{
	public:
		void OnServerStarted(GameServer::NetworkLib::IServer& server) override
		{
			std::cout << "EchoServer started. backend=" << static_cast<int>(server.GetBackendKind()) << "\n";
		}

		void OnClientConnected(std::uint64_t sessionId) override
		{
			std::cout << "client connected. sessionId=" << sessionId << "\n";
		}

		void OnPacketReceived(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const char* buffer, std::int32_t length) override
		{
			std::string message(buffer, buffer + length);
			std::cout << "received: " << message << "\n";
			server.Send(sessionId, buffer, length);
		}

		void OnClientDisconnected(std::uint64_t sessionId) override
		{
			std::cout << "client disconnected. sessionId=" << sessionId << "\n";
		}

		void OnServerStopped() override
		{
			std::cout << "EchoServer stopped.\n";
		}
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

	FEchoApplication echoApplication;
	std::unique_ptr<GameServer::NetworkLib::IServer> server = GameServer::NetworkLib::FServerFactory::Create(serverConfig.backendKind);
	if (server == nullptr)
	{
		std::cerr << "server factory failed.\n";
		return 1;
	}

	if (!server->Start(serverConfig, echoApplication))
	{
		std::cerr << "server start failed.\n";
		return 1;
	}

	std::cout << "Press Enter to stop server...";
	std::cin.get();
	server->Stop();
	return 0;
}
