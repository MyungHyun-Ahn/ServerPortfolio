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

namespace GameServer::Generated::Echo
{
	class FEchoRq final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 1000;

		std::string_view message;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(message);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(message);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(message);
		}
	};

	class FEchoRp final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 1001;

		std::string_view message;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(message);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(message);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(message);
		}
	};

	class FEchoNoti final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 1002;

		std::string_view message;

	public:
		std::uint16_t GetOpcode() const noexcept override
		{
			return kOpcode;
		}

		std::size_t GetEstimatedBodySize() const noexcept override
		{
			return GameServer::NetworkLib::Packet::GetSerializedSize(message);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(message);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(message);
		}
	};

}
