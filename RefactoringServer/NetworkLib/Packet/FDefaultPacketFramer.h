#pragma once

#include "Packet/IPacketFramer.h"

namespace GameServer::NetworkLib::Packet
{
	class FDefaultPacketFramer final : public IPacketFramer
	{
	public:
		bool BuildPacket(const SOutgoingPacket& packet, std::vector<char>& outPacket) const override;
		bool BuildPacketParts(const SOutgoingPacket& packet, SFramedPacketBufferParts& outPacketParts) const override;
		bool TryExtractPacket(std::vector<char>& ioBuffer, SFramedPacket& outPacket) const override;
		bool TryExtractPacket(FRecvBuffer& ioBuffer, SFramedPacket& outPacket) const override;
		bool TryExtractPacketView(FRecvBuffer& ioBuffer, FPacketView& outPacketView) const override;
		std::uint32_t GetHeaderSize() const noexcept override;
	};
}
