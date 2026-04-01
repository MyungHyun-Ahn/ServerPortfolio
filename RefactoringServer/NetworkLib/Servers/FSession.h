#pragma once

#include "Containers/FLockFreeQueue.h"
#include "Packet/FRecvBuffer.h"
#include "Servers/FSendBuffer.h"

#include <WinSock2.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace GameServer::NetworkLib
{
	class FSession
	{
	public:
		enum class EIoType : std::uint32_t
		{
			Recv,
			Send
		};

		struct SIoContext
		{
			OVERLAPPED overlapped{};
			EIoType ioType = EIoType::Recv;
			FSession* ownerSession = nullptr;

			void Prepare(EIoType newIoType, FSession* newOwnerSession);
		};

	public:
		FSession() = default;
		~FSession();

		void Initialize(SOCKET socket, std::uint64_t sessionId, std::uint32_t slotIndex, std::uint32_t generation, std::size_t recvBufferCapacity);
		void Reset() noexcept;

		SOCKET GetSocket() const noexcept;
		void SetSocket(SOCKET socket) noexcept;

		std::uint64_t GetSessionId() const noexcept;
		std::uint32_t GetSlotIndex() const noexcept;
		std::uint32_t GetGeneration() const noexcept;

		bool IsClosing() const noexcept;
		bool TryMarkClosing() noexcept;

		long AcquireRef() noexcept;
		long ReleaseRef() noexcept;

		void BuildRecvWsabufs(WSABUF (&outBuffers)[2], DWORD& outBufferCount) noexcept;
		bool CommitRecvBytes(std::size_t length) noexcept;
		Packet::FRecvBuffer& GetRecvBuffer() noexcept;
		const Packet::FRecvBuffer& GetRecvBuffer() const noexcept;

		SIoContext& GetRecvContext() noexcept;
		SIoContext& GetSendContext() noexcept;

		void EnqueueSendBuffer(FSendBuffer* sendBuffer) noexcept;
		bool TryBeginSend() noexcept;
		void EndSend() noexcept;
		int BeginSendIo() noexcept;
		int FinishSendIo() noexcept;
		int GetMaxObservedConcurrentSendIoCount() const noexcept;
		bool FillSendBatch(std::size_t maxSendCount) noexcept;
		const std::vector<WSABUF>& GetSendWsabufs() const noexcept;
		void ReleaseActiveSendBuffers() noexcept;

	private:
		void ReleaseQueuedSendBuffers() noexcept;

	private:
		inline static constexpr std::size_t kDefaultSendBatchCapacity = 32;
		SOCKET m_socket = INVALID_SOCKET;
		std::uint64_t m_sessionId = 0;
		std::uint32_t m_slotIndex = 0;
		std::uint32_t m_generation = 0;
		std::atomic<long> m_refCount = 1;
		std::atomic<bool> m_closing = false;
		SIoContext m_recvContext{};
		SIoContext m_sendContext{};
		Packet::FRecvBuffer m_recvBuffer;
		Containers::FLockFreeQueue<FSendBuffer*> m_sendQueue;
		std::vector<FSendBuffer*> m_activeSendBuffers;
		std::vector<WSABUF> m_sendWsabufs;
		std::atomic<bool> m_sendInFlight = false;
		std::atomic<int> m_liveSendIoCount = 0;
		std::atomic<int> m_maxObservedConcurrentSendIoCount = 0;
	};
}
