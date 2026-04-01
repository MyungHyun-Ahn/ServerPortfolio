#pragma once

#include "Packet/FPacketSerialization.h"
#include "Packet/IContentPacket.h"

#include <cstdint>
#include <string>
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

}
