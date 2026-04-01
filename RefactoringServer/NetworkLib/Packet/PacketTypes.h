#pragma once

#include <cstdint>
#include <vector>

namespace GameServer::NetworkLib::Packet
{
	enum class EPacketFlags : std::uint8_t
	{
		None = 0
	};

#pragma pack(push, 1)
	struct SPacketHeader
	{
		std::uint16_t payloadLength = 0;
		std::uint8_t randomKey = 0;
		std::uint8_t flags = static_cast<std::uint8_t>(EPacketFlags::None);
	};
#pragma pack(pop)

	struct SFramedPacket
	{
		std::uint8_t randomKey = 0;
		std::uint8_t flags = static_cast<std::uint8_t>(EPacketFlags::None);
		std::vector<char> payload;
	};
}
