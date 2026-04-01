#pragma once

#include <cstdint>

namespace GameServer::NetworkLib
{
	class IServer;

	class IApplicationHandler
	{
	public:
		virtual ~IApplicationHandler() = default;

		virtual void OnServerStarted(IServer& server) = 0;
		virtual void OnClientConnected(std::uint64_t sessionId) = 0;
		virtual void OnPacketReceived(IServer& server, std::uint64_t sessionId, const char* buffer, std::int32_t length) = 0;
		virtual void OnClientDisconnected(std::uint64_t sessionId) = 0;
		virtual void OnServerStopped() = 0;
	};
}
