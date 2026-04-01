#include "Pch.h"

#include "Servers/FSession.h"

namespace GameServer::NetworkLib
{
	FSession::~FSession()
	{
		Reset();
	}

	void FSession::SIoContext::Prepare(EIoType newIoType, FSession* newOwnerSession)
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		ioType = newIoType;
		ownerSession = newOwnerSession;
	}

	void FSession::Initialize(SOCKET socket, std::uint64_t sessionId, std::uint32_t slotIndex, std::uint32_t generation, std::size_t recvBufferCapacity)
	{
		m_socket = socket;
		m_sessionId = sessionId;
		m_slotIndex = slotIndex;
		m_generation = generation;
		m_refCount.store(1);
		m_closing.store(false);
		m_recvBuffer.Initialize(recvBufferCapacity);
		m_activeSendBuffers.clear();
		m_sendWsabufs.clear();
		m_sendInFlight.store(false);
		m_liveSendIoCount.store(0);
		m_maxObservedConcurrentSendIoCount.store(0);
		m_recvContext = {};
		m_sendContext = {};
	}

	void FSession::Reset() noexcept
	{
		ReleaseActiveSendBuffers();
		ReleaseQueuedSendBuffers();
		m_socket = INVALID_SOCKET;
		m_sessionId = 0;
		m_slotIndex = 0;
		m_generation = 0;
		m_refCount.store(1);
		m_closing.store(false);
		m_recvContext = {};
		m_sendContext = {};
		m_recvBuffer.Clear();
		m_sendInFlight.store(false);
		m_liveSendIoCount.store(0);
		m_maxObservedConcurrentSendIoCount.store(0);
	}

	SOCKET FSession::GetSocket() const noexcept
	{
		return m_socket;
	}

	void FSession::SetSocket(SOCKET socket) noexcept
	{
		m_socket = socket;
	}

	std::uint64_t FSession::GetSessionId() const noexcept
	{
		return m_sessionId;
	}

	std::uint32_t FSession::GetSlotIndex() const noexcept
	{
		return m_slotIndex;
	}

	std::uint32_t FSession::GetGeneration() const noexcept
	{
		return m_generation;
	}

	bool FSession::IsClosing() const noexcept
	{
		return m_closing.load();
	}

	bool FSession::TryMarkClosing() noexcept
	{
		bool expected = false;
		return m_closing.compare_exchange_strong(expected, true);
	}

	long FSession::AcquireRef() noexcept
	{
		return m_refCount.fetch_add(1) + 1;
	}

	long FSession::ReleaseRef() noexcept
	{
		return m_refCount.fetch_sub(1) - 1;
	}

	void FSession::BuildRecvWsabufs(WSABUF (&outBuffers)[2], DWORD& outBufferCount) noexcept
	{
		m_recvBuffer.BuildRecvWsabufs(outBuffers, outBufferCount);
	}

	bool FSession::CommitRecvBytes(std::size_t length) noexcept
	{
		return m_recvBuffer.CommitWrite(length);
	}

	Packet::FRecvBuffer& FSession::GetRecvBuffer() noexcept
	{
		return m_recvBuffer;
	}

	const Packet::FRecvBuffer& FSession::GetRecvBuffer() const noexcept
	{
		return m_recvBuffer;
	}

	FSession::SIoContext& FSession::GetRecvContext() noexcept
	{
		return m_recvContext;
	}

	FSession::SIoContext& FSession::GetSendContext() noexcept
	{
		return m_sendContext;
	}

	void FSession::EnqueueSendBuffer(FSendBuffer* sendBuffer) noexcept
	{
		m_sendQueue.Enqueue(sendBuffer);
	}

	bool FSession::TryBeginSend() noexcept
	{
		bool expected = false;
		return m_sendInFlight.compare_exchange_strong(expected, true);
	}

	void FSession::EndSend() noexcept
	{
		m_sendInFlight.store(false);
	}

	int FSession::BeginSendIo() noexcept
	{
		const int liveSendIoCount = m_liveSendIoCount.fetch_add(1) + 1;
		int currentMax = m_maxObservedConcurrentSendIoCount.load();
		while (liveSendIoCount > currentMax &&
			!m_maxObservedConcurrentSendIoCount.compare_exchange_weak(currentMax, liveSendIoCount))
		{
		}

		return liveSendIoCount;
	}

	int FSession::FinishSendIo() noexcept
	{
		return m_liveSendIoCount.fetch_sub(1) - 1;
	}

	int FSession::GetMaxObservedConcurrentSendIoCount() const noexcept
	{
		return m_maxObservedConcurrentSendIoCount.load();
	}

	bool FSession::FillSendBatch(std::size_t maxSendCount) noexcept
	{
		m_activeSendBuffers.clear();
		m_sendWsabufs.clear();

		m_activeSendBuffers.reserve(maxSendCount);
		m_sendWsabufs.reserve(maxSendCount);

		while (m_activeSendBuffers.size() < maxSendCount)
		{
			FSendBuffer* sendBuffer = nullptr;
			if (!m_sendQueue.Dequeue(&sendBuffer))
			{
				break;
			}

			m_activeSendBuffers.push_back(sendBuffer);
			m_sendWsabufs.push_back(sendBuffer->MakeWsabuf());
		}

		return !m_activeSendBuffers.empty();
	}

	const std::vector<WSABUF>& FSession::GetSendWsabufs() const noexcept
	{
		return m_sendWsabufs;
	}

	void FSession::ReleaseActiveSendBuffers() noexcept
	{
		for (FSendBuffer* sendBuffer : m_activeSendBuffers)
		{
			delete sendBuffer;
		}

		m_activeSendBuffers.clear();
		m_sendWsabufs.clear();
	}

	void FSession::ReleaseQueuedSendBuffers() noexcept
	{
		FSendBuffer* sendBuffer = nullptr;
		while (m_sendQueue.Dequeue(&sendBuffer))
		{
			delete sendBuffer;
			sendBuffer = nullptr;
		}
	}
}
