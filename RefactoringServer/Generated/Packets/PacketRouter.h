#pragma once

#include "Generated/Packets/Chat/ChatPacketHandler.h"
#include "Generated/Packets/Chatting/ChattingPacketHandler.h"
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

		void SetChattingHandler(Chatting::IChattingPacketDispatcher* handler) noexcept
		{
			m_chattingHandler = handler;
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
			case Chat::FRoomListRq::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomListRp::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomEnterRq::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomEnterRp::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomChangeRq::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chat::FRoomChangeRp::kOpcode:
				return m_chatHandler != nullptr ? m_chatHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FRoomListRq::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FRoomListRp::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FRoomChangeRq::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FRoomChangeRp::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FChattingRq::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FChattingRp::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Chatting::FBroadcast::kOpcode:
				return m_chattingHandler != nullptr ? m_chattingHandler->DispatchPacket(server, sessionId, packetView) : false;
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
			case Login::FLoginAuthRq::kOpcode:
				return m_loginHandler != nullptr ? m_loginHandler->DispatchPacket(server, sessionId, packetView) : false;
			case Login::FLoginAuthRp::kOpcode:
				return m_loginHandler != nullptr ? m_loginHandler->DispatchPacket(server, sessionId, packetView) : false;
			default:
				return false;
			}
		}

	private:
		Chat::IChatPacketDispatcher* m_chatHandler = nullptr;
		Chatting::IChattingPacketDispatcher* m_chattingHandler = nullptr;
		Echo::IEchoPacketDispatcher* m_echoHandler = nullptr;
		Login::ILoginPacketDispatcher* m_loginHandler = nullptr;
	};
}
