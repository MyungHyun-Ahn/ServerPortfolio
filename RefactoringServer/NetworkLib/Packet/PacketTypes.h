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
		std::uint16_t opcode = 0;
		std::uint16_t payloadLength = 0;
		std::uint8_t randomKey = 0;
		std::uint8_t checkSum = 0;
		std::uint8_t flags = static_cast<std::uint8_t>(EPacketFlags::None);
	};
#pragma pack(pop)

	struct SOutgoingPacket
	{
		std::uint16_t opcode = 0;
		std::uint8_t randomKey = 0;
		std::uint8_t checkSum = 0;
		std::uint8_t flags = static_cast<std::uint8_t>(EPacketFlags::None);
		const char* payload = nullptr;
		std::int32_t payloadLength = 0;
	};

	struct SFramedPacket
	{
		std::uint16_t opcode = 0;
		std::uint8_t randomKey = 0;
		std::uint8_t checkSum = 0;
		std::uint8_t flags = static_cast<std::uint8_t>(EPacketFlags::None);
		std::vector<char> payload;
	};

	inline std::uint8_t CalculatePacketChecksum(const char* payload, std::int32_t payloadLength) noexcept
	{
		if (payload == nullptr || payloadLength <= 0)
		{
			return 0;
		}

		std::uint32_t sum = 0;
		for (std::int32_t index = 0; index < payloadLength; ++index)
		{
			sum += static_cast<std::uint8_t>(payload[index]);
		}

		return static_cast<std::uint8_t>(sum & 0xFF);
	}
}
