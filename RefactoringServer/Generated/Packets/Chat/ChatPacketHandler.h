#pragma once

#include "Generated/Packets/Chat/ChatPackets.h"
#include "Packet/FPacketSerialization.h"
#include "Packet/FPacketView.h"
#include "Servers/IServer.h"

#include <cstdint>

namespace GameServer::Generated::Chat
{
	class IChatPacketHandler
	{
	public:
		virtual ~IChatPacketHandler() = default;

		virtual bool HandleRoomSnapshotRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FRoomSnapshotRq& packet) = 0;
		virtual bool HandleRoomSnapshotRp(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FRoomSnapshotRp& packet) = 0;
	};

	class IChatPacketDispatcher
	{
	public:
		virtual ~IChatPacketDispatcher() = default;
		virtual bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) = 0;
	};

	class FChatPacketHandlerBase : public IChatPacketHandler, public IChatPacketDispatcher
	{
	public:
		bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) override
		{
			switch (packetView.opcode)
			{
			case FRoomSnapshotRq::kOpcode:
				{
					FRoomSnapshotRq packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleRoomSnapshotRq(server, sessionId, packet);
				}
			case FRoomSnapshotRp::kOpcode:
				{
					FRoomSnapshotRp packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleRoomSnapshotRp(server, sessionId, packet);
				}
			default:
				return OnUnhandledPacket(server, sessionId, packetView);
			}
		}

		bool HandleRoomSnapshotRq(GameServer::NetworkLib::IServer&, std::uint64_t, const FRoomSnapshotRq&) override
		{
			return false;
		}

		bool HandleRoomSnapshotRp(GameServer::NetworkLib::IServer&, std::uint64_t, const FRoomSnapshotRp&) override
		{
			return false;
		}

	protected:
		virtual bool OnUnhandledPacket(GameServer::NetworkLib::IServer&, std::uint64_t, const GameServer::NetworkLib::Packet::FPacketView&)
		{
			return false;
		}
	};

	template <typename TPacket>
	inline bool SendGeneratedPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const TPacket& packet)
	{
		return GameServer::NetworkLib::Packet::SendContentPacket(server, sessionId, packet);
	}
}
