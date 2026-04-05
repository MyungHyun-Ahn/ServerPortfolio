#pragma once

namespace NetworkLib::Packet::Buffer
{
	class FSendBuffer
	{
	public:
		inline static constexpr std::size_t kDefaultPageSize = 4096;

		FSendBuffer() = default;

		void Initialize(std::vector<char>&& buffer) noexcept
		{
			m_headerLength = 0;
			m_buffer = std::move(buffer);
		}

		void Initialize(const NetworkLib::Packet::Framing::SFramedPacketBufferParts& packetParts, std::vector<char>&& payloadBuffer) noexcept
		{
			m_headerBytes = packetParts.headerBytes;
			m_headerLength = packetParts.headerLength;
			m_buffer = std::move(payloadBuffer);
		}

		void Reset() noexcept
		{
			m_headerLength = 0;
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

		static FSendBuffer* Create(NetworkLib::Packet::Buffer::FPacketBuffer* packetBuffer) noexcept
		{
			if (packetBuffer == nullptr)
			{
				return nullptr;
			}

			FSendBuffer* sendBuffer = s_sendBufferPool.Alloc();
			sendBuffer->Initialize(std::move(packetBuffer->GetBuffer()));
			NetworkLib::Packet::Buffer::FPacketBuffer::Release(packetBuffer);
			return sendBuffer;
		}

		static FSendBuffer* Create(const NetworkLib::Packet::Framing::SFramedPacketBufferParts& packetParts, std::vector<char>&& payloadBuffer) noexcept
		{
			FSendBuffer* sendBuffer = s_sendBufferPool.Alloc();
			sendBuffer->Initialize(packetParts, std::move(payloadBuffer));
			return sendBuffer;
		}

		static FSendBuffer* Create(
			const NetworkLib::Packet::Framing::SFramedPacketBufferParts& packetParts,
			NetworkLib::Packet::Buffer::FPacketBuffer* packetBuffer) noexcept
		{
			if (packetBuffer == nullptr)
			{
				return nullptr;
			}

			FSendBuffer* sendBuffer = s_sendBufferPool.Alloc();
			sendBuffer->Initialize(packetParts, std::move(packetBuffer->GetBuffer()));
			NetworkLib::Packet::Buffer::FPacketBuffer::Release(packetBuffer);
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

		void AppendWsabufs(std::vector<WSABUF>& outWsabufs) noexcept
		{
			if (m_headerLength > 0)
			{
				WSABUF headerWsabuf{};
				headerWsabuf.buf = m_headerBytes.data();
				headerWsabuf.len = m_headerLength;
				outWsabufs.push_back(headerWsabuf);
			}

			if (!m_buffer.empty())
			{
				WSABUF payloadWsabuf{};
				payloadWsabuf.buf = m_buffer.data();
				payloadWsabuf.len = static_cast<ULONG>(m_buffer.size());
				outWsabufs.push_back(payloadWsabuf);
			}
		}

	private:
		std::array<char, sizeof(NetworkLib::Packet::Framing::SPacketHeader)> m_headerBytes{};
		ULONG m_headerLength = 0;
		std::vector<char> m_buffer;
		inline static std::atomic<bool> s_pageReuseEnabled{ true };
		inline static std::atomic<std::size_t> s_pageSize{ kDefaultPageSize };
		inline static NetworkLib::Memory::FTlsMemoryPoolManager<FSendBuffer, 256, 2> s_sendBufferPool{};
	};
}
