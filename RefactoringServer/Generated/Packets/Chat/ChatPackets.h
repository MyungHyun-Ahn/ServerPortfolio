#pragma once

#include "Packet/Serialization/FPacketSerialization.h"
#include "Packet/Serialization/IContentPacket.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <array>

namespace Generated::Chat
{
	class FRoomSnapshotRq final : public NetworkLib::Packet::Serialization::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 3000;

		std::uint32_t roomId;

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
			return NetworkLib::Packet::Serialization::GetSerializedSize(roomId);
		}

		void Serialize(NetworkLib::Packet::Serialization::FPacketWriter& writer) const override
		{
			writer.Write(roomId);
		}

		bool Deserialize(NetworkLib::Packet::Serialization::FPacketReader& reader) override
		{
			return reader.Read(roomId);
		}
	};

	class FRoomSnapshotRp final : public NetworkLib::Packet::Serialization::IContentPacket
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

		bool ContainsBorrowedViews() const noexcept override
		{
			return false;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return NetworkLib::Packet::Serialization::GetSerializedSize(roomId)
				+ NetworkLib::Packet::Serialization::GetSerializedSize(participants)
				+ NetworkLib::Packet::Serialization::GetSerializedSize(unreadCounts)
				+ NetworkLib::Packet::Serialization::GetSerializedSize(metadata);
		}

		void Serialize(NetworkLib::Packet::Serialization::FPacketWriter& writer) const override
		{
			writer.Write(roomId);
			writer.Write(participants);
			writer.Write(unreadCounts);
			writer.Write(metadata);
		}

		bool Deserialize(NetworkLib::Packet::Serialization::FPacketReader& reader) override
		{
			return reader.Read(roomId)
				&& reader.Read(participants)
				&& reader.Read(unreadCounts)
				&& reader.Read(metadata);
		}
	};

	class FRoomBinarySnapshotNoti final : public NetworkLib::Packet::Serialization::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 3002;

		std::uint32_t roomId;
		void SetPayloadValue(std::span<const std::uint8_t> value) noexcept
		{
			m_payload = value;
		}

		std::span<const std::uint8_t> GetPayloadValue() const noexcept
		{
			NetworkLib::Packet::View::ValidateBorrowedViewAccess(m_borrowedViewScope);
			return m_payload;
		}


		void BindBorrowedViewScope(const std::shared_ptr<NetworkLib::Packet::View::FBorrowedViewScopeState>& scope) noexcept override
		{
			m_borrowedViewScope = scope;
		}

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		bool ContainsBorrowedViews() const noexcept override
		{
			return true;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return NetworkLib::Packet::Serialization::GetSerializedSize(roomId)
				+ NetworkLib::Packet::Serialization::GetSerializedSize(m_payload);
		}

		void Serialize(NetworkLib::Packet::Serialization::FPacketWriter& writer) const override
		{
			writer.Write(roomId);
			writer.Write(m_payload);
		}

		bool Deserialize(NetworkLib::Packet::Serialization::FPacketReader& reader) override
		{
			return reader.Read(roomId)
				&& reader.Read(m_payload);
		}

	private:
		std::shared_ptr<NetworkLib::Packet::View::FBorrowedViewScopeState> m_borrowedViewScope;
		std::span<const std::uint8_t> m_payload;
	};

}
