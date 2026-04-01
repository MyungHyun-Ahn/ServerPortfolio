#pragma once

#include "Packet/IPacketFramer.h"

namespace GameServer::NetworkLib::Packet
{
	class FDefaultPacketFramer final : public IPacketFramer
	{
	public:
		bool BuildPacket(const char* payload, std::int32_t payloadLength, std::uint8_t randomKey, std::vector<char>& outPacket) const override;
		bool TryExtractPacket(std::vector<char>& ioBuffer, SFramedPacket& outPacket) const override;
		std::uint32_t GetHeaderSize() const noexcept override;
	};
}
