#pragma once

#include "Packet/PacketTypes.h"

namespace GameServer::NetworkLib::Packet
{
	class IPacketFramer
	{
	public:
		virtual ~IPacketFramer() = default;

	public:
		virtual bool BuildPacket(const SOutgoingPacket& packet, std::vector<char>& outPacket) const = 0;
		virtual bool TryExtractPacket(std::vector<char>& ioBuffer, SFramedPacket& outPacket) const = 0;
		virtual std::uint32_t GetHeaderSize() const noexcept = 0;
	};
}
