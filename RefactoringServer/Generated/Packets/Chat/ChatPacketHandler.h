#pragma once

#include "Generated/Packets/Chat/ChatPackets.h"
#include "Packet/Serialization/FPacketSerialization.h"
#include "Packet/View/FPacketView.h"
#include "Servers/IServer.h"

#include <cstdint>

namespace Generated::Chat
{
	class IChatPacketHandler
	{
	public:
		virtual ~IChatPacketHandler() = default;

		virtual bool HandleRoomSnapshotRq(NetworkLib::IServer& server, std::uint64_t sessionId, const FRoomSnapshotRq& packet) = 0;
		virtual bool HandleRoomSnapshotRp(NetworkLib::IServer& server, std::uint64_t sessionId, const FRoomSnapshotRp& packet) = 0;
		virtual bool HandleRoomBinarySnapshotNoti(NetworkLib::IServer& server, std::uint64_t sessionId, const FRoomBinarySnapshotNoti& packet) = 0;
	};

	class IChatPacketDispatcher
	{
	public:
		virtual ~IChatPacketDispatcher() = default;
		virtual bool DispatchPacket(NetworkLib::IServer& server, std::uint64_t sessionId, const NetworkLib::Packet::View::FPacketView& packetView) = 0;
	};

	class FChatPacketHandlerBase : public IChatPacketHandler, public IChatPacketDispatcher
	{
	public:
		bool DispatchPacket(NetworkLib::IServer& server, std::uint64_t sessionId, const NetworkLib::Packet::View::FPacketView& packetView) override
		{
			switch (packetView.opcode)
			{
			case FRoomSnapshotRq::kOpcode:
				{
					FRoomSnapshotRq packet;
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleRoomSnapshotRq(server, sessionId, packet);
				}
			case FRoomSnapshotRp::kOpcode:
				{
					FRoomSnapshotRp packet;
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleRoomSnapshotRp(server, sessionId, packet);
				}
			case FRoomBinarySnapshotNoti::kOpcode:
				{
					FRoomBinarySnapshotNoti packet;
					NetworkLib::Packet::View::FBorrowedViewScope borrowedViewScope;
					packet.BindBorrowedViewScope(borrowedViewScope.GetState());
					if (!NetworkLib::Packet::Serialization::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleRoomBinarySnapshotNoti(server, sessionId, packet);
				}
			default:
				return OnUnhandledPacket(server, sessionId, packetView);
			}
		}

		bool HandleRoomSnapshotRq(NetworkLib::IServer&, std::uint64_t, const FRoomSnapshotRq&) override
		{
			return false;
		}

		bool HandleRoomSnapshotRp(NetworkLib::IServer&, std::uint64_t, const FRoomSnapshotRp&) override
		{
			return false;
		}

		bool HandleRoomBinarySnapshotNoti(NetworkLib::IServer&, std::uint64_t, const FRoomBinarySnapshotNoti&) override
		{
			return false;
		}

	protected:
		virtual bool OnUnhandledPacket(NetworkLib::IServer&, std::uint64_t, const NetworkLib::Packet::View::FPacketView&)
		{
			return false;
		}
	};

	template <typename TPacket>
	inline bool SendGeneratedPacket(NetworkLib::IServer& server, std::uint64_t sessionId, const TPacket& packet)
	{
		return NetworkLib::Packet::Serialization::SendContentPacket(server, sessionId, packet);
	}
}
