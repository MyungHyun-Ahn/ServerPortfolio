#include "Pch.h"

#include "Servers/Session/FRioSession.h"

namespace NetworkLib::Session
{
	FRioSession* FRioSession::Create() noexcept
	{
		FRioSession* session = s_sessionPool.Alloc();
		session->Reset();
		return session;
	}

	void FRioSession::Destroy(FRioSession* session) noexcept
	{
		if (session == nullptr)
		{
			return;
		}

		session->Reset();
		s_sessionPool.Free(session);
	}

	LONG FRioSession::GetPoolCapacity() noexcept
	{
		return s_sessionPool.GetCapacity();
	}

	LONG FRioSession::GetPoolUsage() noexcept
	{
		return s_sessionPool.GetUseCount();
	}

	FRioSession::~FRioSession()
	{
		Reset();
	}

	void FRioSession::Initialize(std::uint64_t sessionId, std::uint32_t slotIndex, std::uint32_t generation) noexcept
	{
		m_sessionId = sessionId;
		m_slotIndex = slotIndex;
		m_generation = generation;
		m_refCount.store(1);
		m_closing.store(false);
		m_queuedSendBufferCount.store(0);
		m_maxObservedQueuedSendBufferCount.store(0);
	}

	void FRioSession::Reset() noexcept
	{
		m_sessionId = 0;
		m_slotIndex = 0;
		m_generation = 0;
		m_refCount.store(1);
		m_closing.store(false);
		m_queuedSendBufferCount.store(0);
		m_maxObservedQueuedSendBufferCount.store(0);
	}

	std::uint64_t FRioSession::GetSessionId() const noexcept
	{
		return m_sessionId;
	}

	std::uint32_t FRioSession::GetSlotIndex() const noexcept
	{
		return m_slotIndex;
	}

	std::uint32_t FRioSession::GetGeneration() const noexcept
	{
		return m_generation;
	}

	bool FRioSession::IsClosing() const noexcept
	{
		return m_closing.load();
	}

	bool FRioSession::TryMarkClosing() noexcept
	{
		bool expected = false;
		return m_closing.compare_exchange_strong(expected, true);
	}

	long FRioSession::AcquireRef() noexcept
	{
		return m_refCount.fetch_add(1) + 1;
	}

	long FRioSession::ReleaseRef() noexcept
	{
		return m_refCount.fetch_sub(1) - 1;
	}

	std::uint32_t FRioSession::GetQueuedSendBufferCount() const noexcept
	{
		return m_queuedSendBufferCount.load();
	}

	std::uint32_t FRioSession::GetMaxObservedQueuedSendBufferCount() const noexcept
	{
		return m_maxObservedQueuedSendBufferCount.load();
	}
}

