#pragma once

#include "Packet/ContentHeader.h"
#include "Packet/FPacketReader.h"
#include "Packet/FPacketView.h"
#include "Packet/FPacketWriter.h"
#include "Servers/IServer.h"

#include <vector>

namespace GameServer::NetworkLib::Packet
{
	template <typename TPacket>
	inline std::vector<char> SerializeContentBody(const TPacket& packet)
	{
		FPacketWriter writer;
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
		std::vector<char> payload = SerializeContentBody(packet);
		return server.Send(
			sessionId,
			packet.GetOpcode(),
			payload.empty() ? nullptr : payload.data(),
			static_cast<std::int32_t>(payload.size()));
	}
}
