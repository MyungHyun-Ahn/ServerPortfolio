#pragma once

#include "Memory/FTlsMemoryPool.h"

#include <vector>

namespace GameServer::NetworkLib::Packet
{
	class FPacketBuffer
	{
	public:
		FPacketBuffer() = default;

		void Reset() noexcept
		{
			m_buffer.clear();
		}

		static FPacketBuffer* Create() noexcept
		{
			FPacketBuffer* packetBuffer = s_packetBufferPool.Alloc();
			packetBuffer->Reset();
			return packetBuffer;
		}

		static void Release(FPacketBuffer* packetBuffer) noexcept
		{
			if (packetBuffer == nullptr)
			{
				return;
			}

			packetBuffer->Reset();
			s_packetBufferPool.Free(packetBuffer);
		}

		std::vector<char>& GetBuffer() noexcept
		{
			return m_buffer;
		}

		const std::vector<char>& GetBuffer() const noexcept
		{
			return m_buffer;
		}

	private:
		std::vector<char> m_buffer;
		inline static Memory::FTlsMemoryPoolManager<FPacketBuffer, 256, 2> s_packetBufferPool{};
	};
}
