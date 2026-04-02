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

		void SetMessageValue(std::string_view value) noexcept
		{
			m_message = value;
		}

		std::string_view GetMessageValue() const noexcept
		{
			GameServer::NetworkLib::Packet::ValidateBorrowedViewAccess(m_borrowedViewScope);
			return m_message;
		}


		void BindBorrowedViewScope(const std::shared_ptr<GameServer::NetworkLib::Packet::FBorrowedViewScopeState>& scope) noexcept override
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
			return GameServer::NetworkLib::Packet::GetSerializedSize(m_message);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(m_message);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(m_message);
		}

	private:
		std::shared_ptr<GameServer::NetworkLib::Packet::FBorrowedViewScopeState> m_borrowedViewScope;
		std::string_view m_message;
	};

	class FEchoRp final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 1001;

		void SetMessageValue(std::string_view value) noexcept
		{
			m_message = value;
		}

		std::string_view GetMessageValue() const noexcept
		{
			GameServer::NetworkLib::Packet::ValidateBorrowedViewAccess(m_borrowedViewScope);
			return m_message;
		}


		void BindBorrowedViewScope(const std::shared_ptr<GameServer::NetworkLib::Packet::FBorrowedViewScopeState>& scope) noexcept override
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
			return GameServer::NetworkLib::Packet::GetSerializedSize(m_message);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(m_message);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(m_message);
		}

	private:
		std::shared_ptr<GameServer::NetworkLib::Packet::FBorrowedViewScopeState> m_borrowedViewScope;
		std::string_view m_message;
	};

	class FEchoNoti final : public GameServer::NetworkLib::Packet::IContentPacket
	{
	public:
		static constexpr std::uint16_t kOpcode = 1002;

		void SetMessageValue(std::string_view value) noexcept
		{
			m_message = value;
		}

		std::string_view GetMessageValue() const noexcept
		{
			GameServer::NetworkLib::Packet::ValidateBorrowedViewAccess(m_borrowedViewScope);
			return m_message;
		}


		void BindBorrowedViewScope(const std::shared_ptr<GameServer::NetworkLib::Packet::FBorrowedViewScopeState>& scope) noexcept override
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
			return GameServer::NetworkLib::Packet::GetSerializedSize(m_message);
		}

		void Serialize(GameServer::NetworkLib::Packet::FPacketWriter& writer) const override
		{
			writer.Write(m_message);
		}

		bool Deserialize(GameServer::NetworkLib::Packet::FPacketReader& reader) override
		{
			return reader.Read(m_message);
		}

	private:
		std::shared_ptr<GameServer::NetworkLib::Packet::FBorrowedViewScopeState> m_borrowedViewScope;
		std::string_view m_message;
	};

}
