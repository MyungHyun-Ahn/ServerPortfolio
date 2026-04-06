#pragma once

#include "Servers/Session/ISession.h"

namespace NetworkLib::Packet::Buffer
{
	class FSendBuffer;
}

namespace NetworkLib::Session
{
	class FRioSession final : public ISession
	{
	public:
		static FRioSession* Create() noexcept;
		static void Destroy(FRioSession* session) noexcept;
		static void EnsurePoolCapacity(LONG targetCapacity) noexcept;
		static LONG GetPoolCapacity() noexcept;
		static LONG GetPoolUsage() noexcept;

	public:
		enum class ERequestKind : std::uint8_t
		{
			Recv = 0,
			Send = 1
		};

		struct SRequestContext
		{
			ERequestKind requestKind = ERequestKind::Recv;
			FRioSession* ownerSession = nullptr;
		};

		struct SRecvRequestContext final : SRequestContext
		{
			RIO_BUF buffer{};
		};

		struct SSendRequestContext final : SRequestContext
		{
			NetworkLib::Packet::Buffer::FSendBuffer* sendBuffer = nullptr;
			RIO_BUFFERID bufferId = RIO_INVALID_BUFFERID;
			bool ownsBufferRegistration = false;
			RIO_BUF buffer{};
		};

	public:
		FRioSession() = default;
		~FRioSession() override;

		void Initialize(
			SOCKET socket,
			std::uint64_t sessionId,
			std::uint32_t slotIndex,
			std::uint32_t generation,
			std::uint32_t ownerWorkerIndex,
			std::size_t recvBufferCapacity,
			std::size_t recvStagingCapacity) noexcept;
		void Reset() noexcept;

		SOCKET GetSocket() const noexcept;
		void SetSocket(SOCKET socket) noexcept;

		std::uint64_t GetSessionId() const noexcept override;
		std::uint32_t GetSlotIndex() const noexcept override;
		std::uint32_t GetGeneration() const noexcept override;
		std::uint32_t GetOwnerWorkerIndex() const noexcept;

		RIO_RQ GetRequestQueue() const noexcept;
		void SetRequestQueue(RIO_RQ requestQueue) noexcept;
		RIO_BUFFERID GetRecvBufferId() const noexcept;
		void SetRecvBufferId(RIO_BUFFERID bufferId) noexcept;
		std::mutex& GetRequestQueueMutex() noexcept;

		NetworkLib::Packet::Buffer::FRecvBuffer& GetRecvBuffer() noexcept;
		const NetworkLib::Packet::Buffer::FRecvBuffer& GetRecvBuffer() const noexcept;
		char* GetRecvStagingData() noexcept;
		std::size_t GetRecvStagingCapacity() const noexcept;
		bool CopyReceivedDataFromStaging(std::size_t length) noexcept;

		SRecvRequestContext& GetRecvRequestContext() noexcept;
		const SRecvRequestContext& GetRecvRequestContext() const noexcept;

		bool IsClosing() const noexcept override;
		bool TryMarkClosing() noexcept override;
		bool TryBeginRecv() noexcept;
		void EndRecv() noexcept;

		long AcquireRef() noexcept override;
		long ReleaseRef() noexcept override;

		void OnSendQueued() noexcept;
		void OnSendCompleted() noexcept;
		std::uint32_t GetQueuedSendBufferCount() const noexcept override;
		std::uint32_t GetMaxObservedQueuedSendBufferCount() const noexcept override;

		void ReleaseRioResources(const RIO_EXTENSION_FUNCTION_TABLE& rioFunctionTable) noexcept;

	private:
		SOCKET m_socket = INVALID_SOCKET;
		std::uint64_t m_sessionId = 0;
		std::uint32_t m_slotIndex = 0;
		std::uint32_t m_generation = 0;
		std::uint32_t m_ownerWorkerIndex = 0;
		RIO_RQ m_requestQueue = RIO_INVALID_RQ;
		RIO_BUFFERID m_recvBufferId = RIO_INVALID_BUFFERID;
		std::atomic<long> m_refCount = 1;
		std::atomic<bool> m_closing = false;
		std::atomic<bool> m_recvPending = false;
		std::atomic<std::uint32_t> m_queuedSendBufferCount = 0;
		std::atomic<std::uint32_t> m_maxObservedQueuedSendBufferCount = 0;
		NetworkLib::Packet::Buffer::FRecvBuffer m_recvBuffer;
		std::vector<char> m_recvStagingBuffer;
		SRecvRequestContext m_recvRequestContext{};
		std::mutex m_requestQueueMutex;

	private:
		inline static NetworkLib::Memory::FTlsMemoryPoolManager<FRioSession, 128, 2> s_sessionPool{};
	};
}
