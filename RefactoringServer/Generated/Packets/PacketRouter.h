#pragma once

#include "Generated/Packets/Chat/ChatPacketHandler.h"
#include "Generated/Packets/Echo/EchoPacketHandler.h"
#include "Generated/Packets/Login/LoginPacketHandler.h"
#include "Packet/View/FPacketView.h"
#include "Servers/IServer.h"

#include <cstdint>

namespace Generated
{
	class FPacketRouter
	{
	public:
		void SetChatHandler(Chat::IChatPacketDispatcher* handler) noexcept
		{
			m_chatHandler = handler;
		}

		void SetEchoHandler(Echo::IEchoPacketDispatcher* handler) noexcept
		{
			m_echoHandler = handler;
		}

		void SetLoginHandler(Login::ILoginPacketDispatcher* handler) noexcept
		{
			m_loginHandler = handler;
		}

		bool DispatchPacket(NetworkLib::IServer& server, std::uint64_t sessionId, const NetworkLib::Packet::View::FPacketView& packetView)
		{
			switch (packetView.opcode)
			{
			case Chat::FRoomSnapshotRq::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomSnapshotRp::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomBinarySnapshotNoti::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
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
		Chat::IChatPacketDispatcher* m_chatHandler = nullptr;
		Echo::IEchoPacketDispatcher* m_echoHandler = nullptr;
		Login::ILoginPacketDispatcher* m_loginHandler = nullptr;
	};
}
