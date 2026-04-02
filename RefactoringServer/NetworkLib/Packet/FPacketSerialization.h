#pragma once

#include "Packet/ContentHeader.h"
#include "Packet/FPacketReader.h"
#include "Packet/FPacketView.h"
#include "Packet/FPacketWriter.h"
#include "Servers/IServer.h"

#include <span>
#include <string_view>
#include <vector>

namespace GameServer::NetworkLib::Packet
{
	template <typename TValue>
	inline std::size_t GetSerializedSize(const TValue&) noexcept
		requires CPacketWritableScalar<TValue>
	{
		return sizeof(TValue);
	}

	inline std::size_t GetSerializedSize(const std::string& value) noexcept
	{
		return sizeof(std::uint32_t) + value.size();
	}

	inline std::size_t GetSerializedSize(const std::string_view value) noexcept
	{
		return sizeof(std::uint32_t) + value.size();
	}

	inline std::size_t GetSerializedSize(const std::span<const std::uint8_t> value) noexcept
	{
		return sizeof(std::uint32_t) + value.size_bytes();
	}

	template <typename TValue>
	inline std::size_t GetSerializedSize(const std::vector<TValue>& values) noexcept
	{
		std::size_t totalSize = sizeof(std::uint32_t);
		if constexpr (CPacketWritableScalar<TValue>)
		{
			return totalSize + sizeof(TValue) * values.size();
		}

		for (const TValue& value : values)
		{
			totalSize += GetSerializedSize(value);
		}

		return totalSize;
	}

	template <typename TValue, std::size_t N>
	inline std::size_t GetSerializedSize(const std::array<TValue, N>& values) noexcept
	{
		if constexpr (CPacketWritableScalar<TValue>)
		{
			return sizeof(TValue) * values.size();
		}

		std::size_t totalSize = 0;
		for (const TValue& value : values)
		{
			totalSize += GetSerializedSize(value);
		}

		return totalSize;
	}

	template <typename TKey, typename TValue, typename TCompare, typename TAllocator>
	inline std::size_t GetSerializedSize(const std::map<TKey, TValue, TCompare, TAllocator>& values) noexcept
	{
		std::size_t totalSize = sizeof(std::uint32_t);
		for (const auto& [key, value] : values)
		{
			totalSize += GetSerializedSize(key);
			totalSize += GetSerializedSize(value);
		}

		return totalSize;
	}

	template <typename TKey, typename TValue, typename THash, typename TKeyEqual, typename TAllocator>
	inline std::size_t GetSerializedSize(const std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>& values) noexcept
	{
		std::size_t totalSize = sizeof(std::uint32_t);
		for (const auto& [key, value] : values)
		{
			totalSize += GetSerializedSize(key);
			totalSize += GetSerializedSize(value);
		}

		return totalSize;
	}

	template <typename TPacket>
	inline std::vector<char> SerializeContentBody(const TPacket& packet)
	{
		FPacketWriter writer;
		writer.ReserveAdditional(packet.GetEstimatedBodySize());
		packet.Serialize(writer);
		return writer.MoveBuffer();
	}

	template <typename TPacket>
	inline bool DeserializeContentPacket(const char* payload, std::size_t payloadLength, TPacket& outPacket)
	{
		FPacketReader reader(payload, payloadLength);
		return outPacket.Deserialize(reader) && reader.IsAtEnd();
	}

	template <typename TPacket>
	inline bool DeserializeContentPacket(const FPacketView& packetView, TPacket& outPacket)
	{
		if (packetView.opcode != TPacket::kOpcode)
		{
			return false;
		}

		return DeserializeContentPacket(packetView.payload, static_cast<std::size_t>(packetView.payloadLength), outPacket);
	}

	inline std::vector<char> BuildContentPayload(std::uint16_t opcode, std::vector<char>&& bodyBuffer)
	{
		SContentHeader contentHeader{};
		contentHeader.opcode = opcode;

		std::vector<char> payloadBuffer;
		payloadBuffer.resize(sizeof(SContentHeader) + bodyBuffer.size());
		std::memcpy(payloadBuffer.data(), &contentHeader, sizeof(SContentHeader));
		if (!bodyBuffer.empty())
		{
			std::memcpy(payloadBuffer.data() + sizeof(SContentHeader), bodyBuffer.data(), bodyBuffer.size());
		}

		return payloadBuffer;
	}

	template <typename TPacket>
	inline std::vector<char> SerializeContentPacket(const TPacket& packet)
	{
		return BuildContentPayload(packet.GetOpcode(), SerializeContentBody(packet));
	}

	inline bool TryParseContentPacketView(const FPacketView& transportPacketView, FPacketView& outPacketView)
	{
		if (transportPacketView.payload == nullptr ||
			transportPacketView.payloadLength < static_cast<std::int32_t>(sizeof(SContentHeader)))
		{
			return false;
		}

		SContentHeader contentHeader{};
		std::memcpy(&contentHeader, transportPacketView.payload, sizeof(SContentHeader));

		outPacketView = transportPacketView;
		outPacketView.opcode = contentHeader.opcode;
		outPacketView.payload = transportPacketView.payload + sizeof(SContentHeader);
		outPacketView.payloadLength = transportPacketView.payloadLength - static_cast<std::int32_t>(sizeof(SContentHeader));
		return true;
	}

	template <typename TPacket>
	inline bool SendContentPacket(GameServer::NetworkLib::IServer& server, std::uint64_t sessionId, const TPacket& packet)
	{
		FPacketWriter writer;
		writer.ReserveAdditional(packet.GetEstimatedBodySize());
		packet.Serialize(writer);
		const std::vector<char>& payload = writer.GetBuffer();
		return server.Send(
			sessionId,
			packet.GetOpcode(),
			payload.empty() ? nullptr : payload.data(),
			static_cast<std::int32_t>(payload.size()));
	}
}
