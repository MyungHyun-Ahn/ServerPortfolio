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

	void FRioSession::EnsurePoolCapacity(LONG targetCapacity) noexcept
	{
		if (targetCapacity <= 0)
		{
			return;
		}

		std::vector<FRioSession*> reservedSessions;
		reservedSessions.reserve(static_cast<std::size_t>(targetCapacity));
		while (GetPoolCapacity() < targetCapacity)
		{
			FRioSession* session = s_sessionPool.Alloc();
			if (session == nullptr)
			{
				break;
			}

			session->Reset();
			reservedSessions.push_back(session);
		}

		for (FRioSession* session : reservedSessions)
		{
			s_sessionPool.Free(session);
		}
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

	void FRioSession::Initialize(
		SOCKET socket,
		std::uint64_t sessionId,
		std::uint32_t slotIndex,
		std::uint32_t generation,
		std::uint32_t ownerWorkerIndex,
		std::size_t recvBufferCapacity,
		std::size_t recvStagingCapacity) noexcept
	{
		m_socket = socket;
		m_sessionId = sessionId;
		m_slotIndex = slotIndex;
		m_generation = generation;
		m_ownerWorkerIndex = ownerWorkerIndex;
		m_requestQueue = RIO_INVALID_RQ;
		m_recvBufferId = RIO_INVALID_BUFFERID;
		m_refCount.store(1);
		m_closing.store(false);
		m_recvPending.store(false);
		m_queuedSendBufferCount.store(0);
		m_maxObservedQueuedSendBufferCount.store(0);
		m_recvBuffer.Initialize(recvBufferCapacity);
		m_recvStagingBuffer.assign(recvStagingCapacity, 0);
		m_recvRequestContext = {};
		m_recvRequestContext.requestKind = ERequestKind::Recv;
		m_recvRequestContext.ownerSession = this;
	}

	void FRioSession::Reset() noexcept
	{
		m_socket = INVALID_SOCKET;
		m_sessionId = 0;
		m_slotIndex = 0;
		m_generation = 0;
		m_ownerWorkerIndex = 0;
		m_requestQueue = RIO_INVALID_RQ;
		m_recvBufferId = RIO_INVALID_BUFFERID;
		m_refCount.store(1);
		m_closing.store(false);
		m_recvPending.store(false);
		m_queuedSendBufferCount.store(0);
		m_maxObservedQueuedSendBufferCount.store(0);
		m_recvBuffer.Clear();
		m_recvStagingBuffer.clear();
		m_recvRequestContext = {};
		m_recvRequestContext.requestKind = ERequestKind::Recv;
		m_recvRequestContext.ownerSession = this;
	}

	SOCKET FRioSession::GetSocket() const noexcept
	{
		return m_socket;
	}

	void FRioSession::SetSocket(SOCKET socket) noexcept
	{
		m_socket = socket;
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

	std::uint32_t FRioSession::GetOwnerWorkerIndex() const noexcept
	{
		return m_ownerWorkerIndex;
	}

	RIO_RQ FRioSession::GetRequestQueue() const noexcept
	{
		return m_requestQueue;
	}

	void FRioSession::SetRequestQueue(RIO_RQ requestQueue) noexcept
	{
		m_requestQueue = requestQueue;
	}

	RIO_BUFFERID FRioSession::GetRecvBufferId() const noexcept
	{
		return m_recvBufferId;
	}

	void FRioSession::SetRecvBufferId(RIO_BUFFERID bufferId) noexcept
	{
		m_recvBufferId = bufferId;
	}

	std::mutex& FRioSession::GetRequestQueueMutex() noexcept
	{
		return m_requestQueueMutex;
	}

	Packet::Buffer::FRecvBuffer& FRioSession::GetRecvBuffer() noexcept
	{
		return m_recvBuffer;
	}

	const Packet::Buffer::FRecvBuffer& FRioSession::GetRecvBuffer() const noexcept
	{
		return m_recvBuffer;
	}

	char* FRioSession::GetRecvStagingData() noexcept
	{
		return m_recvStagingBuffer.empty() ? nullptr : m_recvStagingBuffer.data();
	}

	std::size_t FRioSession::GetRecvStagingCapacity() const noexcept
	{
		return m_recvStagingBuffer.size();
	}

	bool FRioSession::CopyReceivedDataFromStaging(std::size_t length) noexcept
	{
		if (length > m_recvStagingBuffer.size() || length > m_recvBuffer.GetFreeSize())
		{
			return false;
		}

		WSABUF recvBuffers[2]{};
		DWORD recvBufferCount = 0;
		m_recvBuffer.BuildRecvWsabufs(recvBuffers, recvBufferCount);
		std::size_t copiedLength = 0;
		for (DWORD bufferIndex = 0; bufferIndex < recvBufferCount && copiedLength < length; ++bufferIndex)
		{
			const std::size_t chunkLength = std::min<std::size_t>(recvBuffers[bufferIndex].len, length - copiedLength);
			std::memcpy(recvBuffers[bufferIndex].buf, m_recvStagingBuffer.data() + copiedLength, chunkLength);
			copiedLength += chunkLength;
		}

		return copiedLength == length && m_recvBuffer.CommitWrite(length);
	}

	FRioSession::SRecvRequestContext& FRioSession::GetRecvRequestContext() noexcept
	{
		return m_recvRequestContext;
	}

	const FRioSession::SRecvRequestContext& FRioSession::GetRecvRequestContext() const noexcept
	{
		return m_recvRequestContext;
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

	bool FRioSession::TryBeginRecv() noexcept
	{
		bool expected = false;
		return m_recvPending.compare_exchange_strong(expected, true);
	}

	void FRioSession::EndRecv() noexcept
	{
		m_recvPending.store(false);
	}

	long FRioSession::AcquireRef() noexcept
	{
		return m_refCount.fetch_add(1) + 1;
	}

	long FRioSession::ReleaseRef() noexcept
	{
		return m_refCount.fetch_sub(1) - 1;
	}

	void FRioSession::OnSendQueued() noexcept
	{
		const std::uint32_t queuedCount = m_queuedSendBufferCount.fetch_add(1) + 1;
		std::uint32_t currentMax = m_maxObservedQueuedSendBufferCount.load();
		while (queuedCount > currentMax &&
			!m_maxObservedQueuedSendBufferCount.compare_exchange_weak(currentMax, queuedCount))
		{
		}
	}

	void FRioSession::OnSendCompleted() noexcept
	{
		m_queuedSendBufferCount.fetch_sub(1);
	}

	std::uint32_t FRioSession::GetQueuedSendBufferCount() const noexcept
	{
		return m_queuedSendBufferCount.load();
	}

	std::uint32_t FRioSession::GetMaxObservedQueuedSendBufferCount() const noexcept
	{
		return m_maxObservedQueuedSendBufferCount.load();
	}

	void FRioSession::ReleaseRioResources(const RIO_EXTENSION_FUNCTION_TABLE& rioFunctionTable) noexcept
	{
		if (m_recvBufferId != RIO_INVALID_BUFFERID)
		{
			rioFunctionTable.RIODeregisterBuffer(m_recvBufferId);
			m_recvBufferId = RIO_INVALID_BUFFERID;
		}

		m_requestQueue = RIO_INVALID_RQ;
	}
}
