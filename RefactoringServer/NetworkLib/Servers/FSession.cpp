#include "Pch.h"

#include "Servers/FSession.h"

namespace GameServer::NetworkLib
{
	void FSession::SIoContext::Prepare(EIoType newIoType, FSession* newOwnerSession, std::uint32_t bufferSize)
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		ioType = newIoType;
		ownerSession = newOwnerSession;
		if (bufferSize > 0)
		{
			buffer.resize(bufferSize);
		}

		wsabuf.buf = buffer.empty() ? nullptr : buffer.data();
		wsabuf.len = static_cast<ULONG>(buffer.size());
	}

	void FSession::Initialize(SOCKET socket, std::uint64_t sessionId, std::uint32_t slotIndex, std::uint32_t generation)
	{
		m_socket = socket;
		m_sessionId = sessionId;
		m_slotIndex = slotIndex;
		m_generation = generation;
		m_refCount.store(1);
		m_closing.store(false);
		m_recvBuffer.clear();
		m_recvContext = {};
	}

	void FSession::Reset() noexcept
	{
		m_socket = INVALID_SOCKET;
		m_sessionId = 0;
		m_slotIndex = 0;
		m_generation = 0;
		m_refCount.store(1);
		m_closing.store(false);
		m_recvContext = {};
		m_recvBuffer.clear();
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

	void FSession::AppendRecvBytes(const char* buffer, std::size_t length)
	{
		m_recvBuffer.insert(m_recvBuffer.end(), buffer, buffer + length);
	}

	std::vector<char>& FSession::GetRecvBuffer() noexcept
	{
		return m_recvBuffer;
	}

	const std::vector<char>& FSession::GetRecvBuffer() const noexcept
	{
		return m_recvBuffer;
	}

	FSession::SIoContext& FSession::GetRecvContext() noexcept
	{
		return m_recvContext;
	}
}
