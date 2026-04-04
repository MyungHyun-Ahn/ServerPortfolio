#pragma once

#include "Servers/Session/ISession.h"

namespace NetworkLib::Session
{
	class FRioSession final : public ISession
	{
	public:
		static FRioSession* Create() noexcept;
		static void Destroy(FRioSession* session) noexcept;
		static LONG GetPoolCapacity() noexcept;
		static LONG GetPoolUsage() noexcept;

	public:
		FRioSession() = default;
		~FRioSession() override;

		void Initialize(std::uint64_t sessionId, std::uint32_t slotIndex, std::uint32_t generation) noexcept;
		void Reset() noexcept;

		std::uint64_t GetSessionId() const noexcept override;
		std::uint32_t GetSlotIndex() const noexcept override;
		std::uint32_t GetGeneration() const noexcept override;

		bool IsClosing() const noexcept override;
		bool TryMarkClosing() noexcept override;

		long AcquireRef() noexcept override;
		long ReleaseRef() noexcept override;

		std::uint32_t GetQueuedSendBufferCount() const noexcept override;
		std::uint32_t GetMaxObservedQueuedSendBufferCount() const noexcept override;

	private:
		std::uint64_t m_sessionId = 0;
		std::uint32_t m_slotIndex = 0;
		std::uint32_t m_generation = 0;
		std::atomic<long> m_refCount = 1;
		std::atomic<bool> m_closing = false;
		std::atomic<std::uint32_t> m_queuedSendBufferCount = 0;
		std::atomic<std::uint32_t> m_maxObservedQueuedSendBufferCount = 0;

	private:
		inline static NetworkLib::Memory::FTlsMemoryPoolManager<FRioSession, 128, 2> s_sessionPool{};
	};
}

