#pragma once

#include "Memory/FTlsMemoryPool.h"

#include <WinSock2.h>

#include <utility>
#include <vector>

namespace GameServer::NetworkLib
{
	class FSendBuffer
	{
	public:
		FSendBuffer() = default;

		void Initialize(std::vector<char>&& buffer) noexcept
		{
			m_buffer = std::move(buffer);
		}

		void Reset() noexcept
		{
			m_buffer.clear();
		}

		static FSendBuffer* Create(std::vector<char>&& buffer) noexcept
		{
			FSendBuffer* sendBuffer = s_sendBufferPool.Alloc();
			sendBuffer->Initialize(std::move(buffer));
			return sendBuffer;
		}

		static void Release(FSendBuffer* sendBuffer) noexcept
		{
			if (sendBuffer == nullptr)
			{
				return;
			}

			sendBuffer->Reset();
			s_sendBufferPool.Free(sendBuffer);
		}

		static LONG GetPoolCapacity() noexcept
		{
			return s_sendBufferPool.GetCapacity();
		}

		static LONG GetPoolUsage() noexcept
		{
			return s_sendBufferPool.GetUseCount();
		}

		const char* GetData() const noexcept
		{
			return m_buffer.data();
		}

		char* GetData() noexcept
		{
			return m_buffer.data();
		}

		std::size_t GetSize() const noexcept
		{
			return m_buffer.size();
		}

		WSABUF MakeWsabuf() noexcept
		{
			WSABUF wsabuf{};
			wsabuf.buf = m_buffer.empty() ? nullptr : m_buffer.data();
			wsabuf.len = static_cast<ULONG>(m_buffer.size());
			return wsabuf;
		}

	private:
		std::vector<char> m_buffer;
		inline static Memory::FTlsMemoryPoolManager<FSendBuffer, 256, 2> s_sendBufferPool{};
	};
}
