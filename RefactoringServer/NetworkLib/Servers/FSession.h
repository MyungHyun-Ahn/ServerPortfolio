#pragma once

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
			WSABUF wsabuf{};
			EIoType ioType = EIoType::Recv;
			FSession* ownerSession = nullptr;
			std::vector<char> buffer;

			void Prepare(EIoType newIoType, FSession* newOwnerSession, std::uint32_t bufferSize = 0);
		};

	public:
		FSession() = default;
		~FSession() = default;

		void Initialize(SOCKET socket, std::uint64_t sessionId, std::uint32_t slotIndex, std::uint32_t generation);
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

		void AppendRecvBytes(const char* buffer, std::size_t length);
		std::vector<char>& GetRecvBuffer() noexcept;
		const std::vector<char>& GetRecvBuffer() const noexcept;

		SIoContext& GetRecvContext() noexcept;

	private:
		SOCKET m_socket = INVALID_SOCKET;
		std::uint64_t m_sessionId = 0;
		std::uint32_t m_slotIndex = 0;
		std::uint32_t m_generation = 0;
		std::atomic<long> m_refCount = 1;
		std::atomic<bool> m_closing = false;
		SIoContext m_recvContext{};
		std::vector<char> m_recvBuffer;
	};
}
