#pragma once

#include "Packet/FPacketView.h"

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
		// packetView는 현재 콜백 범위 안에서만 유효하다.
		virtual void OnPacketReceived(IServer& server, std::uint64_t sessionId, const Packet::FPacketView& packetView) = 0;
		virtual void OnClientDisconnected(std::uint64_t sessionId) = 0;
		virtual void OnServerStopped() = 0;
	};
}
