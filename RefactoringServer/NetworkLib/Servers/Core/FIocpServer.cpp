#include "Pch.h"

#include "Crypto/IPacketCipher.h"
#include "Packet/Buffer/FPacketBuffer.h"
#include "Packet/Serialization/FPacketSerialization.h"
#include "Packet/Framing/IPacketFramer.h"
#include "Servers/Core/FIocpServer.h"
#include "Servers/IApplicationHandler.h"
#include "Servers/Session/FIocpSession.h"
#include "Foundation/Logging/ILogger.h"

#pragma comment(lib, "Ws2_32.lib")

namespace NetworkLib::Core
{
	using NetworkLib::Packet::Buffer::FPacketBuffer;
	using NetworkLib::Packet::Buffer::FSendBuffer;
	using NetworkLib::Packet::Framing::CalculatePacketChecksum;
	using NetworkLib::Packet::Framing::SFramedPacketBufferParts;
	using NetworkLib::Packet::Framing::SOutgoingPacket;
	using NetworkLib::Packet::Framing::SPacketHeader;
	using NetworkLib::Packet::Serialization::TryParseContentPacketView;
	using NetworkLib::Packet::View::FPacketView;
	using NetworkLib::Session::FIocpSession;

	namespace
	{
		bool ApplyAcceptedSocketSendBufferOption(
			const SServerConfig& serverConfig,
			const SOCKET clientSocket,
			std::string& outError) noexcept
		{
			if (serverConfig.socketSendBufferBytes < 0)
			{
				return true;
			}

			const int sendBufferBytes = serverConfig.socketSendBufferBytes;
			if (setsockopt(
				clientSocket,
				SOL_SOCKET,
				SO_SNDBUF,
				reinterpret_cast<const char*>(&sendBufferBytes),
				sizeof(sendBufferBytes)) == SOCKET_ERROR)
			{
				std::ostringstream oss;
				oss << "setsockopt(SO_SNDBUF) failed. requested=" << sendBufferBytes
					<< " error=" << WSAGetLastError();
				outError = oss.str();
				return false;
			}

			return true;
		}
	}

	FIocpServer::FIocpServer() = default;

	FIocpServer::~FIocpServer()
	{
		Stop();
	}

	bool FIocpServer::Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler)
	{
		if (m_isRunning.exchange(true))
		{
			Log(Foundation::ELogLevel::Warn, "Start requested while server is already running.");
			return false;
		}

		m_serverConfig = serverConfig;
		m_applicationHandler = &applicationHandler;
		m_logger = m_serverConfig.logger;
		m_packetCipher = m_serverConfig.packetCipher;
		m_packetFramer = m_serverConfig.packetFramer;
		FSendBuffer::ConfigurePageReuse(m_serverConfig.enablePageBufferReuse, m_serverConfig.pageBufferSize);
		FPacketBuffer::ConfigurePageReuse(
			m_serverConfig.enablePageBufferReuse,
			m_serverConfig.pageBufferSize);
		if (m_packetCipher != nullptr && m_packetFramer == nullptr)
		{
			Log(Foundation::ELogLevel::Error, "Packet cipher requires packet framer.");
			m_isRunning = false;
			return false;
		}

		m_sessionSlots = std::make_unique<std::atomic<FIocpSession*>[]>(m_serverConfig.maxSessionCount);
		m_generations = std::make_unique<std::atomic<std::uint32_t>[]>(m_serverConfig.maxSessionCount);
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			m_sessionSlots[slotIndex].store(nullptr);
			m_generations[slotIndex].store(1);
		}

		if (!InitializeWinsock())
		{
			Log(Foundation::ELogLevel::Error, "Winsock initialization failed.");
			Stop();
			return false;
		}

		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
		if (m_iocpHandle == nullptr)
		{
			std::ostringstream oss;
			oss << "CreateIoCompletionPort failed. error=" << GetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			Stop();
			return false;
		}

		if (!OpenListenSocket())
		{
			Log(Foundation::ELogLevel::Error, "Listen socket open failed.");
			Stop();
			return false;
		}

		if (CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(m_listenSocket),
			m_iocpHandle,
			kAcceptCompletionKey,
			0) == nullptr)
		{
			std::ostringstream oss;
			oss << "CreateIoCompletionPort listen attach failed. error=" << GetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			Stop();
			return false;
		}

		if (!LoadAcceptExFunctions())
		{
			Log(Foundation::ELogLevel::Error, "AcceptEx extension function load failed.");
			Stop();
			return false;
		}

		if (!InitializeAcceptContexts())
		{
			Log(Foundation::ELogLevel::Error, "AcceptEx context initialization failed.");
			Stop();
			return false;
		}

		StartWorkers();
		const std::uint32_t workerCount = std::max(1u, m_serverConfig.workerThreadCount);
		{
			std::ostringstream oss;
			oss << "Server started. ip=" << m_serverConfig.bindIp
				<< " port=" << m_serverConfig.port
				<< " workers=" << workerCount
				<< " maxSessions=" << m_serverConfig.maxSessionCount
				<< " acceptContexts=" << m_acceptContextCount;
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnServerStarted(*this);
		return true;
	}

	void FIocpServer::Stop()
	{
		if (!m_isRunning.exchange(false))
		{
			return;
		}

		Log(Foundation::ELogLevel::Info, "Server stop requested.");

		CloseListenSocket();

		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FIocpSession* sessionContext = m_sessionSlots[slotIndex].exchange(nullptr);
			if (sessionContext != nullptr)
			{
				CloseSession(*sessionContext);
				ReleaseSession(sessionContext);
			}
		}

		StopWorkers();
		CloseAcceptContexts();

		if (m_iocpHandle != nullptr)
		{
			CloseHandle(m_iocpHandle);
			m_iocpHandle = nullptr;
		}

		if (m_winsockInitialized.exchange(false))
		{
			WSACleanup();
		}

		if (m_applicationHandler != nullptr)
		{
			m_applicationHandler->OnServerStopped();
			m_applicationHandler = nullptr;
		}

		Log(Foundation::ELogLevel::Info, "Server stopped.");
		m_packetCipher.reset();
		m_packetFramer.reset();
		m_logger.reset();
	}

	bool FIocpServer::SendPacket(
		std::uint64_t sessionId,
		NetworkLib::Packet::Serialization::FOutgoingContentPacket&& packet)
	{
		if (!packet.IsValid())
		{
			Log(Foundation::ELogLevel::Warn, "Send rejected because outgoing packet was invalid.");
			return false;
		}

		const std::int32_t bodyLength = packet.GetBodyLength();
		FIocpSession* sessionContext = AcquireSession(sessionId);
		if (sessionContext == nullptr)
		{
			std::ostringstream oss;
			oss << "Send rejected because session was not found. sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Warn, oss.str());
			return false;
		}

		if (m_packetFramer != nullptr)
		{
			std::vector<char> payloadBuffer = packet.MoveBuffer();
			std::uint8_t randomKey = 0;
			if (m_packetCipher != nullptr)
			{
				randomKey = GeneratePacketRandomKey();
				m_packetCipher->Encode(payloadBuffer.data(), static_cast<int>(payloadBuffer.size()), randomKey);
			}

			SOutgoingPacket outgoingPacket{};
			outgoingPacket.randomKey = randomKey;
			outgoingPacket.checkSum =
				CalculatePacketChecksum(
					payloadBuffer.data(),
					static_cast<std::int32_t>(payloadBuffer.size()));
			outgoingPacket.payload = payloadBuffer.data();
			outgoingPacket.payloadLength = static_cast<std::int32_t>(payloadBuffer.size());

			SFramedPacketBufferParts packetParts{};
			if (!m_packetFramer->BuildPacketParts(outgoingPacket, packetParts))
			{
				Log(Foundation::ELogLevel::Error, "BuildPacketParts failed during send path.");
				ReleaseSession(sessionContext);
				return false;
			}

			sessionContext->EnqueueSendBuffer(FSendBuffer::Create(packetParts, std::move(payloadBuffer)));
		}
		else
		{
			std::vector<char> payloadBuffer = packet.MoveBuffer();
			sessionContext->EnqueueSendBuffer(FSendBuffer::Create(std::move(payloadBuffer)));
		}
		m_sentPacketCount.fetch_add(1, std::memory_order_relaxed);
		m_sentByteCount.fetch_add(static_cast<std::uint64_t>(bodyLength > 0 ? bodyLength : 0), std::memory_order_relaxed);
		PostSend(*sessionContext);

		ReleaseSession(sessionContext);
		return true;
	}

	bool FIocpServer::Disconnect(std::uint64_t sessionId)
	{
		FIocpSession* sessionContext = AcquireSession(sessionId);
		if (sessionContext == nullptr)
		{
			return false;
		}

		CloseSession(*sessionContext);
		ReleaseSession(sessionContext);
		return true;
	}

	EBackendKind FIocpServer::GetBackendKind() const
	{
		return EBackendKind::Iocp;
	}

	SServerStats FIocpServer::GetStatsSnapshot() const
	{
		SServerStats stats{};
		stats.activeSessionCount = m_activeSessionCount.load(std::memory_order_relaxed);
		stats.acceptedSessionCount = m_acceptedSessionCount.load(std::memory_order_relaxed);
		stats.receivedPacketCount = m_receivedPacketCount.load(std::memory_order_relaxed);
		stats.sentPacketCount = m_sentPacketCount.load(std::memory_order_relaxed);
		stats.receivedByteCount = m_receivedByteCount.load(std::memory_order_relaxed);
		stats.sentByteCount = m_sentByteCount.load(std::memory_order_relaxed);
		stats.wsaRecvCallCount = m_wsaRecvCallCount.load(std::memory_order_relaxed);
		stats.wsaSendCallCount = m_wsaSendCallCount.load(std::memory_order_relaxed);
		stats.sessionPoolCapacity = static_cast<std::uint32_t>(FIocpSession::GetPoolCapacity());
		stats.sessionPoolUsage = static_cast<std::uint32_t>(FIocpSession::GetPoolUsage());
		stats.sendBufferPoolCapacity = static_cast<std::uint32_t>(FSendBuffer::GetPoolCapacity());
		stats.sendBufferPoolUsage = static_cast<std::uint32_t>(FSendBuffer::GetPoolUsage());
		stats.packetBufferPoolCapacity = static_cast<std::uint32_t>(FPacketBuffer::GetPoolCapacity());
		stats.packetBufferPoolUsage = static_cast<std::uint32_t>(FPacketBuffer::GetPoolUsage());

		std::uint64_t queuedSendBufferCount = 0;
		std::uint64_t maxObservedQueuedSendBufferCount = 0;
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FIocpSession* sessionContext = m_sessionSlots[slotIndex].load(std::memory_order_relaxed);
			if (sessionContext == nullptr)
			{
				continue;
			}

			queuedSendBufferCount += sessionContext->GetQueuedSendBufferCount();
			maxObservedQueuedSendBufferCount = std::max<std::uint64_t>(
				maxObservedQueuedSendBufferCount,
				sessionContext->GetMaxObservedQueuedSendBufferCount());
		}
		stats.queuedSendBufferCount = queuedSendBufferCount;
		stats.maxObservedQueuedSendBufferCount = maxObservedQueuedSendBufferCount;
		return stats;
	}

	bool FIocpServer::InitializeWinsock()
	{
		WSADATA wsaData{};
		const int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (startupResult != 0)
		{
			std::ostringstream oss;
			oss << "WSAStartup failed. error=" << startupResult;
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		m_winsockInitialized = true;
		return true;
	}

	bool FIocpServer::OpenListenSocket()
	{
		m_listenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (m_listenSocket == INVALID_SOCKET)
		{
			std::ostringstream oss;
			oss << "WSASocketW failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		sockaddr_in listenAddress{};
		listenAddress.sin_family = AF_INET;
		listenAddress.sin_port = htons(m_serverConfig.port);
		if (InetPtonA(AF_INET, m_serverConfig.bindIp.c_str(), &listenAddress.sin_addr) != 1)
		{
			std::ostringstream oss;
			oss << "InetPtonA failed for bind ip. ip=" << m_serverConfig.bindIp;
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		BOOL reuseAddress = TRUE;
		setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

		if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&listenAddress), sizeof(listenAddress)) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "bind failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "listen failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		return true;
	}

	bool FIocpServer::LoadAcceptExFunctions()
	{
		DWORD bytesReturned = 0;
		GUID acceptExGuid = WSAID_ACCEPTEX;
		if (WSAIoctl(
			m_listenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&acceptExGuid,
			sizeof(acceptExGuid),
			&m_acceptEx,
			sizeof(m_acceptEx),
			&bytesReturned,
			nullptr,
			nullptr) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "WSAIoctl(AcceptEx) failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		GUID getAcceptExSockaddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;
		if (WSAIoctl(
			m_listenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&getAcceptExSockaddrsGuid,
			sizeof(getAcceptExSockaddrsGuid),
			&m_getAcceptExSockaddrs,
			sizeof(m_getAcceptExSockaddrs),
			&bytesReturned,
			nullptr,
			nullptr) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "WSAIoctl(GetAcceptExSockaddrs) failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		return true;
	}

	bool FIocpServer::InitializeAcceptContexts()
	{
		if (m_serverConfig.maxSessionCount == 0)
		{
			Log(Foundation::ELogLevel::Error, "AcceptEx initialization requires maxSessionCount > 0.");
			return false;
		}

		const std::uint32_t desiredAcceptContextCount =
			std::max(kMinimumAcceptContextCount, std::max(1u, m_serverConfig.workerThreadCount) * 2u);
		m_acceptContextCount = std::min(m_serverConfig.maxSessionCount, desiredAcceptContextCount);
		m_acceptContexts = std::make_unique<SAcceptContext[]>(m_acceptContextCount);
		for (std::uint32_t acceptSlotIndex = 0; acceptSlotIndex < m_acceptContextCount; ++acceptSlotIndex)
		{
			m_acceptContexts[acceptSlotIndex].slotIndex = acceptSlotIndex;
			m_acceptContexts[acceptSlotIndex].acceptedSocket = INVALID_SOCKET;
			m_acceptContexts[acceptSlotIndex].ResetOverlapped();

			if (!PostAccept(acceptSlotIndex))
			{
				return false;
			}
		}

		return true;
	}

	bool FIocpServer::PostAccept(std::uint32_t acceptSlotIndex)
	{
		if (acceptSlotIndex >= m_acceptContextCount || m_acceptEx == nullptr)
		{
			return false;
		}

		SAcceptContext& acceptContext = m_acceptContexts[acceptSlotIndex];
		if (acceptContext.acceptedSocket != INVALID_SOCKET)
		{
			closesocket(acceptContext.acceptedSocket);
			acceptContext.acceptedSocket = INVALID_SOCKET;
		}

		acceptContext.acceptedSocket =
			WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (acceptContext.acceptedSocket == INVALID_SOCKET)
		{
			std::ostringstream oss;
			oss << "WSASocketW for AcceptEx failed. slot=" << acceptSlotIndex
				<< " error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		acceptContext.ResetOverlapped();

		DWORD bytesReceived = 0;
		const BOOL acceptResult =
			m_acceptEx(
				m_listenSocket,
				acceptContext.acceptedSocket,
				acceptContext.buffer.data(),
				0,
				static_cast<DWORD>((sizeof(sockaddr_in) + 16)),
				static_cast<DWORD>((sizeof(sockaddr_in) + 16)),
				&bytesReceived,
				&acceptContext.overlapped);
		if (acceptResult == FALSE)
		{
			const int errorCode = WSAGetLastError();
			if (errorCode != WSA_IO_PENDING)
			{
				std::ostringstream oss;
				oss << "AcceptEx post failed. slot=" << acceptSlotIndex
					<< " error=" << errorCode;
				Log(Foundation::ELogLevel::Error, oss.str());
				closesocket(acceptContext.acceptedSocket);
				acceptContext.acceptedSocket = INVALID_SOCKET;
				return false;
			}
		}

		return true;
	}

	void FIocpServer::CloseAcceptContexts() noexcept
	{
		if (m_acceptContexts == nullptr)
		{
			return;
		}

		for (std::uint32_t acceptSlotIndex = 0; acceptSlotIndex < m_acceptContextCount; ++acceptSlotIndex)
		{
			SAcceptContext& acceptContext = m_acceptContexts[acceptSlotIndex];
			if (acceptContext.acceptedSocket != INVALID_SOCKET)
			{
				closesocket(acceptContext.acceptedSocket);
				acceptContext.acceptedSocket = INVALID_SOCKET;
			}
		}

		m_acceptContexts.reset();
		m_acceptContextCount = 0;
	}

	void FIocpServer::CloseListenSocket()
	{
		if (m_listenSocket != INVALID_SOCKET)
		{
			closesocket(m_listenSocket);
			m_listenSocket = INVALID_SOCKET;
		}
	}

	void FIocpServer::StartWorkers()
	{
		const std::uint32_t workerCount = std::max(1u, m_serverConfig.workerThreadCount);
		m_workerThreads.reserve(workerCount);
		for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
		{
			m_workerThreads.emplace_back(&FIocpServer::WorkerLoop, this);
		}
	}

	void FIocpServer::StopWorkers()
	{
		for (std::size_t workerIndex = 0; workerIndex < m_workerThreads.size(); ++workerIndex)
		{
			PostQueuedCompletionStatus(m_iocpHandle, 0, 0, nullptr);
		}

		for (auto& workerThread : m_workerThreads)
		{
			if (workerThread.joinable())
			{
				workerThread.join();
			}
		}

		m_workerThreads.clear();
	}

	bool FIocpServer::HandleAcceptCompletion(
		SAcceptContext& acceptContext,
		bool completionSucceeded,
		DWORD completionError)
	{
		if (acceptContext.acceptedSocket == INVALID_SOCKET)
		{
			return false;
		}

		if (!completionSucceeded)
		{
			if (m_isRunning && completionError != ERROR_OPERATION_ABORTED)
			{
				std::ostringstream oss;
				oss << "AcceptEx completion failed. slot=" << acceptContext.slotIndex
					<< " error=" << completionError;
				Log(Foundation::ELogLevel::Warn, oss.str());
			}

			closesocket(acceptContext.acceptedSocket);
			acceptContext.acceptedSocket = INVALID_SOCKET;
			return false;
		}

		if (!m_isRunning)
		{
			closesocket(acceptContext.acceptedSocket);
			acceptContext.acceptedSocket = INVALID_SOCKET;
			return false;
		}

		if (!AttachAcceptedSocket(acceptContext.acceptedSocket))
		{
			closesocket(acceptContext.acceptedSocket);
			acceptContext.acceptedSocket = INVALID_SOCKET;
			return false;
		}

		acceptContext.acceptedSocket = INVALID_SOCKET;
		return true;
	}

	void FIocpServer::WorkerLoop()
	{
		while (true)
		{
			DWORD transferredBytes = 0;
			ULONG_PTR completionKey = 0;
			LPOVERLAPPED overlapped = nullptr;

			const BOOL queuedResult = GetQueuedCompletionStatus(m_iocpHandle, &transferredBytes, &completionKey, &overlapped, INFINITE);
			const DWORD completionError = queuedResult != FALSE ? ERROR_SUCCESS : GetLastError();
			if (overlapped == nullptr && completionKey == 0)
			{
				break;
			}

			if (completionKey == kAcceptCompletionKey)
			{
				auto* acceptContext = reinterpret_cast<SAcceptContext*>(overlapped);
				if (acceptContext == nullptr)
				{
					continue;
				}

				HandleAcceptCompletion(*acceptContext, queuedResult != FALSE, completionError);
				if (m_isRunning && !PostAccept(acceptContext->slotIndex))
				{
					std::ostringstream oss;
					oss << "AcceptEx repost failed. slot=" << acceptContext->slotIndex;
					Log(Foundation::ELogLevel::Error, oss.str());
				}

				continue;
			}

			auto* ioContext = reinterpret_cast<FIocpSession::SIoContext*>(overlapped);
			FIocpSession* sessionContext = ioContext->ownerSession;
			if (sessionContext == nullptr)
			{
				continue;
			}

			if (queuedResult == FALSE || transferredBytes == 0)
			{
				if (queuedResult == FALSE)
				{
					std::ostringstream oss;
					oss << "I/O completion failed. sessionId=" << sessionContext->GetSessionId() << " error=" << completionError;
					Log(Foundation::ELogLevel::Warn, oss.str());
				}
				if (ioContext->ioType == FIocpSession::EIoType::Send)
				{
					sessionContext->FinishSendIo();
					sessionContext->ReleaseActiveSendBuffers();
					sessionContext->EndSend();
				}
				CloseSession(*sessionContext);
				ReleaseSession(sessionContext);
				continue;
			}

			if (ioContext->ioType == FIocpSession::EIoType::Recv)
			{
				m_receivedByteCount.fetch_add(transferredBytes, std::memory_order_relaxed);
				if (!sessionContext->CommitRecvBytes(transferredBytes))
				{
					Log(Foundation::ELogLevel::Warn, "Recv buffer overflow detected.");
					CloseSession(*sessionContext);
					ReleaseSession(sessionContext);
					continue;
				}

				if (m_packetFramer != nullptr)
				{
					while (true)
					{
						FPacketView packetView;
						if (!m_packetFramer->TryExtractPacketView(sessionContext->GetRecvBuffer(), packetView))
						{
							break;
						}

						const std::uint8_t actualChecksum =
							CalculatePacketChecksum(
								packetView.payload,
								packetView.payloadLength);
						if (actualChecksum != packetView.checkSum)
						{
							std::ostringstream oss;
							oss << "Packet checksum mismatch. sessionId=" << sessionContext->GetSessionId()
								<< " opcode=" << packetView.opcode
								<< " expected=" << static_cast<int>(packetView.checkSum)
								<< " actual=" << static_cast<int>(actualChecksum);
							Log(Foundation::ELogLevel::Warn, oss.str());
							CloseSession(*sessionContext);
							break;
						}

					if (m_packetCipher != nullptr && packetView.payloadLength > 0)
					{
						m_packetCipher->Decode(const_cast<char*>(packetView.payload), packetView.payloadLength, packetView.randomKey);
					}

						FPacketView contentPacketView;
						if (!TryParseContentPacketView(packetView, contentPacketView))
						{
							std::ostringstream oss;
							oss << "Content header parse failed. sessionId=" << sessionContext->GetSessionId();
							Log(Foundation::ELogLevel::Warn, oss.str());
							CloseSession(*sessionContext);
							break;
						}

						m_applicationHandler->OnPacketReceived(
							*this,
							sessionContext->GetSessionId(),
							contentPacketView);
						m_receivedPacketCount.fetch_add(1, std::memory_order_relaxed);

						const std::size_t consumedPacketSize =
							sizeof(SPacketHeader) + static_cast<std::size_t>(packetView.payloadLength);
						sessionContext->GetRecvBuffer().Discard(consumedPacketSize);
					}
				}
				else
				{
					Log(Foundation::ELogLevel::Warn, "Recv path without framer is not supported by ring buffer mode.");
					CloseSession(*sessionContext);
				}

				if (!PostRecv(*sessionContext))
				{
					std::ostringstream oss;
					oss << "PostRecv failed after packet dispatch. sessionId=" << sessionContext->GetSessionId();
					Log(Foundation::ELogLevel::Warn, oss.str());
					CloseSession(*sessionContext);
				}
			}
			else
			{
				sessionContext->FinishSendIo();
				sessionContext->ReleaseActiveSendBuffers();
				const bool sendRestartRequested = sessionContext->EndSend();
				if (!sessionContext->IsClosing())
				{
					if (sendRestartRequested || sessionContext->GetQueuedSendBufferCount() > 0)
					{
						PostSend(*sessionContext);
					}
				}
			}

			ReleaseSession(sessionContext);
		}
	}

bool FIocpServer::PostRecv(FIocpSession& sessionContext)
	{
		DWORD recvFlags = 0;
		DWORD recvBytes = 0;
		WSABUF recvBuffers[2]{};
		DWORD recvBufferCount = 0;

		FIocpSession::SIoContext& recvContext = sessionContext.GetRecvContext();
		recvContext.Prepare(FIocpSession::EIoType::Recv, &sessionContext);
		sessionContext.BuildRecvWsabufs(recvBuffers, recvBufferCount);
		if (recvBufferCount == 0)
		{
			Log(Foundation::ELogLevel::Warn, "PostRecv failed because recv buffer has no writable space.");
			return false;
		}
		sessionContext.AcquireRef();
		m_wsaRecvCallCount.fetch_add(1, std::memory_order_relaxed);

		const int recvResult = WSARecv(sessionContext.GetSocket(), recvBuffers, recvBufferCount, &recvBytes, &recvFlags, &recvContext.overlapped, nullptr);
		if (recvResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "WSARecv failed. sessionId=" << sessionContext.GetSessionId() << " error=" << errorCode;
			Log(Foundation::ELogLevel::Warn, oss.str());
			ReleaseSession(&sessionContext);
			return false;
		}

		return true;
	}

bool FIocpServer::PostSend(FIocpSession& sessionContext)
	{
		while (true)
		{
			if (!sessionContext.TryBeginSend())
			{
				return false;
			}

			if (sessionContext.IsClosing())
			{
				const bool sendRestartRequested = sessionContext.EndSend();
				if (sendRestartRequested)
				{
					continue;
				}
				return false;
			}

			if (!sessionContext.FillSendBatch(kMaxSendBatchCount))
			{
				const bool sendRestartRequested = sessionContext.EndSend();
				if (sendRestartRequested)
				{
					continue;
				}
				return false;
			}

			FIocpSession::SIoContext& sendContext = sessionContext.GetSendContext();
			sendContext.Prepare(FIocpSession::EIoType::Send, &sessionContext);

			sessionContext.AcquireRef();
			const int concurrentSendIoCount = sessionContext.BeginSendIo();
			if (concurrentSendIoCount > 1)
			{
				std::ostringstream oss;
				oss << "Concurrent WSASend detected. sessionId=" << sessionContext.GetSessionId()
					<< " concurrentSendIoCount=" << concurrentSendIoCount;
				Log(Foundation::ELogLevel::Error, oss.str());
				sessionContext.FinishSendIo();
				sessionContext.ReleaseActiveSendBuffers();
				sessionContext.EndSend();
				ReleaseSession(&sessionContext);
				CloseSession(sessionContext);
				return false;
			}

			DWORD sentBytes = 0;
			DWORD sendFlags = 0;
			const std::vector<WSABUF>& sendBuffers = sessionContext.GetSendWsabufs();
			m_wsaSendCallCount.fetch_add(1, std::memory_order_relaxed);
			const int sendResult = WSASend(
				sessionContext.GetSocket(),
				const_cast<WSABUF*>(sendBuffers.data()),
				static_cast<DWORD>(sendBuffers.size()),
				&sentBytes,
				sendFlags,
				&sendContext.overlapped,
				nullptr);

			if (sendResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
			{
				const int errorCode = WSAGetLastError();
				std::ostringstream oss;
				oss << "WSASend failed. sessionId=" << sessionContext.GetSessionId() << " error=" << errorCode;
				Log(Foundation::ELogLevel::Error, oss.str());
				sessionContext.FinishSendIo();
				sessionContext.ReleaseActiveSendBuffers();
				sessionContext.EndSend();
				ReleaseSession(&sessionContext);
				CloseSession(sessionContext);
				return false;
			}

			return true;
		}
	}

	void FIocpServer::CloseSession(FIocpSession& sessionContext)
	{
		if (!sessionContext.TryMarkClosing())
		{
			return;
		}

		shutdown(sessionContext.GetSocket(), SD_BOTH);
		closesocket(sessionContext.GetSocket());
		sessionContext.SetSocket(INVALID_SOCKET);
		m_sessionSlots[sessionContext.GetSlotIndex()].store(nullptr);
		m_activeSessionCount.fetch_sub(1, std::memory_order_relaxed);
		{
			std::ostringstream oss;
			oss << "Session closed. sessionId=" << sessionContext.GetSessionId();
			oss << " maxConcurrentSendIo=" << sessionContext.GetMaxObservedConcurrentSendIoCount();
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnClientDisconnected(sessionContext.GetSessionId());
	}

	void FIocpServer::ReleaseSession(FIocpSession* sessionContext)
	{
		if (sessionContext == nullptr)
		{
			return;
		}

		if (sessionContext->ReleaseRef() == 0)
		{
			FIocpSession::Destroy(sessionContext);
		}
	}

	FIocpSession* FIocpServer::AcquireSession(std::uint64_t sessionId)
	{
		const std::uint32_t slotIndex = static_cast<std::uint32_t>(sessionId & 0xFFFFFFFFULL);
		if (slotIndex >= m_serverConfig.maxSessionCount)
		{
			return nullptr;
		}

		FIocpSession* sessionContext = m_sessionSlots[slotIndex].load();
		if (sessionContext == nullptr || sessionContext->GetSessionId() != sessionId || sessionContext->IsClosing())
		{
			return nullptr;
		}

		sessionContext->AcquireRef();
		if (sessionContext->GetSessionId() != sessionId || sessionContext->IsClosing())
		{
			ReleaseSession(sessionContext);
			return nullptr;
		}

		return sessionContext;
	}

	bool FIocpServer::AttachAcceptedSocket(SOCKET clientSocket)
	{
		if (setsockopt(
			clientSocket,
			SOL_SOCKET,
			SO_UPDATE_ACCEPT_CONTEXT,
			reinterpret_cast<const char*>(&m_listenSocket),
			sizeof(m_listenSocket)) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Warn, oss.str());
			return false;
		}

		{
			std::string errorMessage;
			if (!ApplyAcceptedSocketSendBufferOption(m_serverConfig, clientSocket, errorMessage))
			{
				Log(Foundation::ELogLevel::Error, errorMessage);
				return false;
			}
		}

		FIocpSession* newSessionContext = nullptr;

		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FIocpSession* expected = nullptr;
			FIocpSession* candidateSession = FIocpSession::Create();
			const std::uint32_t generation = m_generations[slotIndex].fetch_add(1);
			const std::uint64_t sessionId = ComposeSessionId(slotIndex, generation);
			const std::size_t recvBufferCapacity =
				static_cast<std::size_t>(std::max<std::uint32_t>(m_serverConfig.recvBufferSize * 8u, 65536u));
			candidateSession->Initialize(
				clientSocket,
				sessionId,
				slotIndex,
				generation,
				recvBufferCapacity);

			if (m_sessionSlots[slotIndex].compare_exchange_strong(expected, candidateSession))
			{
				newSessionContext = candidateSession;
				break;
			}

			FIocpSession::Destroy(candidateSession);
		}

		if (newSessionContext == nullptr)
		{
			Log(Foundation::ELogLevel::Warn, "All session slots are in use.");
			return false;
		}

		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), m_iocpHandle, 0, 0) == nullptr)
		{
			std::ostringstream oss;
			oss << "CreateIoCompletionPort attach failed. error=" << GetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			m_sessionSlots[newSessionContext->GetSlotIndex()].store(nullptr);
			FIocpSession::Destroy(newSessionContext);
			return false;
		}

		{
			std::ostringstream oss;
			oss << "Client connected. sessionId=" << newSessionContext->GetSessionId();
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		m_activeSessionCount.fetch_add(1, std::memory_order_relaxed);
		m_acceptedSessionCount.fetch_add(1, std::memory_order_relaxed);
		m_applicationHandler->OnClientConnected(newSessionContext->GetSessionId());
		if (!PostRecv(*newSessionContext))
		{
			CloseSession(*newSessionContext);
			ReleaseSession(newSessionContext);
			return false;
		}

		return true;
	}

	std::uint64_t FIocpServer::ComposeSessionId(std::uint32_t slotIndex, std::uint32_t generation) const
	{
		return (static_cast<std::uint64_t>(generation) << 32ULL) | static_cast<std::uint64_t>(slotIndex);
	}

	std::uint8_t FIocpServer::GeneratePacketRandomKey() noexcept
	{
		return static_cast<std::uint8_t>(m_packetRandomKeySeed.fetch_add(1, std::memory_order_relaxed) & 0xFF);
	}

	void FIocpServer::Log(Foundation::ELogLevel logLevel, const std::string& message) const
	{
		if (m_logger != nullptr)
		{
			m_logger->Log(logLevel, "NetworkLib", message);
		}
	}
}
