#pragma once

#include "Generated/Packets/Echo/EchoPackets.h"
#include "Packet/FPacketSerialization.h"
#include "Packet/FPacketView.h"
#include "Servers/IServer.h"

#include <cstdint>

namespace GameServer::Generated::Echo
{
	class IEchoPacketHandler
	{
	public:
		virtual ~IEchoPacketHandler() = default;

		virtual bool HandleEchoRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FEchoRq& packet) = 0;
		virtual bool HandleEchoRp(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FEchoRp& packet) = 0;
		virtual bool HandleEchoNoti(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FEchoNoti& packet) = 0;
	};

	class IEchoPacketDispatcher
	{
	public:
		virtual ~IEchoPacketDispatcher() = default;
		virtual bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) = 0;
	};

	class FEchoPacketHandlerBase : public IEchoPacketHandler, public IEchoPacketDispatcher
	{
	public:
		bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) override
		{
			switch (packetView.opcode)
			{
			case FEchoRq::kOpcode:
				{
					FEchoRq packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleEchoRq(server, sessionId, packet);
				}
			case FEchoRp::kOpcode:
				{
					FEchoRp packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleEchoRp(server, sessionId, packet);
				}
			case FEchoNoti::kOpcode:
				{
					FEchoNoti packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleEchoNoti(server, sessionId, packet);
				}
			default:
				return OnUnhandledPacket(server, sessionId, packetView);
			}
		}

		bool HandleEchoRq(GameServer::NetworkLib::IServer&, std::uint64_t, const FEchoRq&) override
		{
			return false;
		}

		bool HandleEchoRp(GameServer::NetworkLib::IServer&, std::uint64_t, const FEchoRp&) override
		{
			return false;
		}

		bool HandleEchoNoti(GameServer::NetworkLib::IServer&, std::uint64_t, const FEchoNoti&) override
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
