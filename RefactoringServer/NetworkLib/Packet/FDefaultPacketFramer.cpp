#include "Pch.h"

#include "Packet/FDefaultPacketFramer.h"

namespace GameServer::NetworkLib::Packet
{
	bool FDefaultPacketFramer::BuildPacket(const char* payload, std::int32_t payloadLength, std::uint8_t randomKey, std::vector<char>& outPacket) const
	{
		if (payload == nullptr || payloadLength < 0 || payloadLength > static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max()))
		{
			return false;
		}

		SPacketHeader packetHeader{};
		packetHeader.payloadLength = static_cast<std::uint16_t>(payloadLength);
		packetHeader.randomKey = randomKey;

		outPacket.resize(sizeof(SPacketHeader) + payloadLength);
		std::memcpy(outPacket.data(), &packetHeader, sizeof(SPacketHeader));

		if (payloadLength > 0)
		{
			std::memcpy(outPacket.data() + sizeof(SPacketHeader), payload, payloadLength);
		}

		return true;
	}

	bool FDefaultPacketFramer::TryExtractPacket(std::vector<char>& ioBuffer, SFramedPacket& outPacket) const
	{
		if (ioBuffer.size() < sizeof(SPacketHeader))
		{
			return false;
		}

		SPacketHeader packetHeader{};
		std::memcpy(&packetHeader, ioBuffer.data(), sizeof(SPacketHeader));

		const std::size_t packetSize = sizeof(SPacketHeader) + packetHeader.payloadLength;
		if (ioBuffer.size() < packetSize)
		{
			return false;
		}

		outPacket.randomKey = packetHeader.randomKey;
		outPacket.flags = packetHeader.flags;
		outPacket.payload.resize(packetHeader.payloadLength);

		if (packetHeader.payloadLength > 0)
		{
			std::memcpy(outPacket.payload.data(), ioBuffer.data() + sizeof(SPacketHeader), packetHeader.payloadLength);
		}

		ioBuffer.erase(ioBuffer.begin(), ioBuffer.begin() + static_cast<std::ptrdiff_t>(packetSize));
		return true;
	}

	std::uint32_t FDefaultPacketFramer::GetHeaderSize() const noexcept
	{
		return static_cast<std::uint32_t>(sizeof(SPacketHeader));
	}
}
