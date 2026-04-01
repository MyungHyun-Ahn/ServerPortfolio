#pragma once

#include "Generated/Packets/Login/LoginPackets.h"
#include "Packet/FPacketSerialization.h"
#include "Packet/FPacketView.h"
#include "Servers/IServer.h"

#include <cstdint>

namespace GameServer::Generated::Login
{
	class ILoginPacketHandler
	{
	public:
		virtual ~ILoginPacketHandler() = default;

		virtual bool HandleLoginRq(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FLoginRq& packet) = 0;
		virtual bool HandleLoginRp(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const FLoginRp& packet) = 0;
	};

	class ILoginPacketDispatcher
	{
	public:
		virtual ~ILoginPacketDispatcher() = default;
		virtual bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) = 0;
	};

	class FLoginPacketHandlerBase : public ILoginPacketHandler, public ILoginPacketDispatcher
	{
	public:
		bool DispatchPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const GameServer::NetworkLib::Packet::FPacketView& packetView) override
		{
			switch (packetView.opcode)
			{
			case FLoginRq::kOpcode:
				{
					FLoginRq packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleLoginRq(server, sessionId, packet);
				}
			case FLoginRp::kOpcode:
				{
					FLoginRp packet;
					if (!GameServer::NetworkLib::Packet::DeserializeContentPacket(packetView, packet))
					{
						return false;
					}

					return HandleLoginRp(server, sessionId, packet);
				}
			default:
				return OnUnhandledPacket(server, sessionId, packetView);
			}
		}

		bool HandleLoginRq(GameServer::NetworkLib::IServer&, std::uint64_t, const FLoginRq&) override
		{
			return false;
		}

		bool HandleLoginRp(GameServer::NetworkLib::IServer&, std::uint64_t, const FLoginRp&) override
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
