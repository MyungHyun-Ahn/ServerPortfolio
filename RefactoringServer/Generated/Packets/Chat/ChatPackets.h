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

namespace GameServer::Generated::Chat
{
	class FRoomSnapshotRq final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 3000;

		std::uint32_t roomId;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(roomId);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(roomId);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(roomId);
		}
	};

	class FRoomSnapshotRp final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 3001;

		std::uint32_t roomId;
		std::vector<std::string> participants;
		std::map<std::string, std::uint32_t> unreadCounts;
		std::unordered_map<std::string, std::string> metadata;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(roomId)
				+ GameServer::NetworkLib::Packet::GetSerializedSize(participants)
				+ GameServer::NetworkLib::Packet::GetSerializedSize(unreadCounts)
				+ GameServer::NetworkLib::Packet::GetSerializedSize(metadata);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(roomId);
			writer.Write(participants);
			writer.Write(unreadCounts);
			writer.Write(metadata);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(roomId)
				&& reader.Read(participants)
				&& reader.Read(unreadCounts)
				&& reader.Read(metadata);
		}
	};

	class FRoomBinarySnapshotNoti final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 3002;

		std::uint32_t roomId;
		std::span<const std::uint8_t> payload;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(roomId)
				+ GameServer::NetworkLib::Packet::GetSerializedSize(payload);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(roomId);
			writer.Write(payload);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(roomId)
				&& reader.Read(payload);
		}
	};

}
