#pragma once

#include "Generated/Packets/Echo/EchoPacketHandler.h"
#include "Generated/Packets/Login/LoginPacketHandler.h"
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

		void SetLoginHandler(Login::ILoginPacketDispatcher* handler) noexcept
		{
			m_loginHandler = handler;
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
			case Login::FLoginRq::kOpcode:
				return m_loginHandler != nullptr ? m_loginHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Login::FLoginRp::kOpcode:
				return m_loginHandler != nullptr ? m_loginHandler->DispatchPacket(server, sessionId, packetView) : false;
			default:
				return false;
			}
		}

	private:
		Echo::IEchoPacketDispatcher* m_echoHandler = nullptr;
		Login::ILoginPacketDispatcher* m_loginHandler = nullptr;
	};
}
