#pragma once

#include "Packet/FPacketSerialization.h"
#include "Packet/IContentPacket.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <array>

namespace GameServer::Generated::Login
{
	class FLoginRq final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 2000;

		std::uint32_t userId;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		bool ContainsBorrowedViews() const noexcept override
		{
			return false;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(userId);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(userId);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(userId);
		}
	};

	class FLoginRp final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 2001;

		std::uint32_t userId;
		bool success;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		bool ContainsBorrowedViews() const noexcept override
		{
			return false;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(userId)
				+ GameServer::NetworkLib::Packet::GetSerializedSize(success);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(userId);
			writer.Write(success);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(userId)
				&& reader.Read(success);
		}
	};

}
