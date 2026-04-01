#pragma once

#include "Packet/FPacketReader.h"
#include "Packet/FPacketWriter.h"

#include <cstdint>

namespace GameServer::NetworkLib::Packet
{
	class IContentPacket
	{
	public:
		virtual ~IContentPacket() = default;

		virtual std::uint16_t GetOpcode() const noexcept = 0;
		virtual void Serialize(FPacketWriter& writer) const = 0;
		virtual bool Deserialize(FPacketReader& reader) = 0;
	};
}
