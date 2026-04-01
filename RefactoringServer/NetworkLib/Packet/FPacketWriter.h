#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace GameServer::NetworkLib::Packet
{
	template <typename TValue>
	concept CPacketWritableScalar =
		std::is_integral_v<TValue> ||
		std::is_floating_point_v<TValue> ||
		std::is_enum_v<TValue>;

	class FPacketWriter
	{
	public:
		FPacketWriter() = default;

	public:
		void WriteBytes(const void* data, std::size_t size)
		{
			if (data == nullptr || size == 0)
			{
				return;
			}

			const std::size_t oldSize = m_buffer.size();
			m_buffer.resize(oldSize + size);
			std::memcpy(m_buffer.data() + oldSize, data, size);
		}

		template <CPacketWritableScalar TValue>
		void Write(const TValue& value)
		{
			WriteBytes(&value, sizeof(TValue));
		}

		void Write(const std::string& value)
		{
			const std::uint32_t length = static_cast<std::uint32_t>(value.size());
			Write(length);
			WriteBytes(value.data(), value.size());
		}

		template <typename TValue>
		void Write(const std::vector<TValue>& values)
		{
			const std::uint32_t count = static_cast<std::uint32_t>(values.size());
			Write(count);
			for (const TValue& value : values)
			{
				Write(value);
			}
		}

		template <typename TValue, std::size_t N>
		void Write(const std::array<TValue, N>& values)
		{
			for (const TValue& value : values)
			{
				Write(value);
			}
		}

		template <typename TKey, typename TValue, typename TCompare, typename TAllocator>
		void Write(const std::map<TKey, TValue, TCompare, TAllocator>& values)
		{
			const std::uint32_t count = static_cast<std::uint32_t>(values.size());
			Write(count);
			for (const auto& [key, value] : values)
			{
				Write(key);
				Write(value);
			}
		}

		template <typename TKey, typename TValue, typename THash, typename TKeyEqual, typename TAllocator>
		void Write(const std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>& values)
		{
			const std::uint32_t count = static_cast<std::uint32_t>(values.size());
			Write(count);
			for (const auto& [key, value] : values)
			{
				Write(key);
				Write(value);
			}
		}

		const std::vector<char>& GetBuffer() const noexcept
		{
			return m_buffer;
		}

		std::vector<char> MoveBuffer() noexcept
		{
			return std::move(m_buffer);
		}

	private:
		std::vector<char> m_buffer;
	};
}
