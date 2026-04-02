#pragma once

#include "Packet/FRecvBuffer.h"
#include "Packet/FPacketView.h"
#include "Packet/PacketTypes.h"

namespace GameServer::NetworkLib::Packet
{
	class IPacketFramer
	{
	public:
		virtual ~IPacketFramer() = default;

	public:
		virtual bool BuildPacket(const SOutgoingPacket& packet, std::vector<char>& outPacket) const = 0;
		virtual bool BuildPacketParts(const SOutgoingPacket& packet, SFramedPacketBufferParts& outPacketParts) const = 0;
		virtual bool TryExtractPacket(std::vector<char>& ioBuffer, SFramedPacket& outPacket) const = 0;
		virtual bool TryExtractPacket(FRecvBuffer& ioBuffer, SFramedPacket& outPacket) const = 0;
		virtual bool TryExtractPacketView(FRecvBuffer& ioBuffer, FPacketView& outPacketView) const = 0;
		virtual std::uint32_t GetHeaderSize() const noexcept = 0;
	};
}
