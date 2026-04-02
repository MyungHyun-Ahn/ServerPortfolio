#pragma once

#include "Packet/FBorrowedViewGuard.h"
#include "Packet/FPacketReader.h"
#include "Packet/FPacketWriter.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace GameServer::NetworkLib::Packet
{
	class IContentPacket
	{
	public:
		virtual ~IContentPacket() = default;

		virtual std::uint16_t GetOpcode() const noexcept = 0;
		virtual bool ContainsBorrowedViews() const noexcept
		{
			return false;
		}

		virtual void BindBorrowedViewScope(const std::shared_ptr<FBorrowedViewScopeState>&) noexcept
		{
		}

		virtual std::size_t GetEstimatedBodySize() const noexcept
		{
			return 0;
		}

		virtual void Serialize(FPacketWriter& writer) const = 0;
		virtual bool Deserialize(FPacketReader& reader) = 0;
	};
}
