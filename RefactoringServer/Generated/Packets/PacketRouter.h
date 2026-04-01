#pragma once

#include "Generated/Packets/Echo/EchoPacketHandler.h"
#include "Packet/FPacketView.h"
#include "Servers/IServer.h"

#include <cstdint>

namespace GameServer::Generated
{
	class FPacketRouter
	{
	public:
		void SetEchoHandler(Echo::IEchoPacketDispatcher* handler) noexcept
		{
			m_echoHandler = handler;
		}

		bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView)
		{
			switch (packetView.opcode)
			{
			case Echo::FEchoRq::kOpcode:
				return m_echoHandler != nullptr ? m_echoHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Echo::FEchoRp::kOpcode:
				return m_echoHandler != nullptr ? m_echoHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Echo::FEchoNoti::kOpcode:
				return m_echoHandler != nullptr ? m_echoHandler->DispatchPacket(server, sessionId, packetView) : false;
			default:
				return false;
			}
		}

	private:
		Echo::IEchoPacketDispatcher* m_echoHandler = nullptr;
	};
}
