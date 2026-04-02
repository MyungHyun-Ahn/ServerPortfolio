#pragma once

#include "Memory/FTlsMemoryPool.h"

#include <WinSock2.h>

#include <atomic>
#include <utility>
#include <vector>

namespace GameServer::NetworkLib
{
	class FSendBuffer
	{
	public:
		inline static constexpr std::size_t kDefaultPageSize = 4096;

		FSendBuffer() = default;

		void Initialize(std::vector<char>&& buffer) noexcept
		{
			m_buffer = std::move(buffer);
		}

		void Reset() noexcept
		{
			if (!IsPageReuseEnabled())
			{
				std::vector<char>().swap(m_buffer);
				return;
			}

			const std::size_t pageSize = GetConfiguredPageSize();
			if (m_buffer.capacity() < pageSize)
			{
				m_buffer.reserve(pageSize);
			}

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

		static void ConfigurePageReuse(bool enabled, std::size_t pageSize = kDefaultPageSize) noexcept
		{
			s_pageReuseEnabled.store(enabled, std::memory_order_relaxed);
			s_pageSize.store(pageSize == 0 ? kDefaultPageSize : pageSize, std::memory_order_relaxed);
		}

		static bool IsPageReuseEnabled() noexcept
		{
			return s_pageReuseEnabled.load(std::memory_order_relaxed);
		}

		static std::size_t GetConfiguredPageSize() noexcept
		{
			return s_pageSize.load(std::memory_order_relaxed);
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
		inline static std::atomic<bool> s_pageReuseEnabled{ true };
		inline static std::atomic<std::size_t> s_pageSize{ kDefaultPageSize };
		inline static Memory::FTlsMemoryPoolManager<FSendBuffer, 256, 2> s_sendBufferPool{};
	};
}
