#include "Pch.h"

#include "Crypto/IPacketCipher.h"
#include "Packet/FPacketBuffer.h"
#include "Packet/FPacketSerialization.h"
#include "Packet/IPacketFramer.h"
#include "Servers/FIocpServer.h"
#include "Servers/IApplicationHandler.h"
#include "Foundation/Logging/ILogger.h"

#pragma comment(lib, "Ws2_32.lib")

namespace GameServer::NetworkLib
{
	FIocpServer::FIocpServer() = default;

	FIocpServer::~FIocpServer()
	{
		Stop();
	}

	bool FIocpServer::Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler)
	{
		if (m_isRunning.exchange(true))
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "Start requested while server is already running.");
			return false;
		}

		m_serverConfig = serverConfig;
		m_applicationHandler = &applicationHandler;
		m_logger = m_serverConfig.logger;
		m_packetCipher = m_serverConfig.packetCipher;
		m_packetFramer = m_serverConfig.packetFramer;
		FSendBuffer::ConfigurePageReuse(m_serverConfig.enablePageBufferReuse, m_serverConfig.pageBufferSize);
		GameServer::NetworkLib::Packet::FPacketBuffer::ConfigurePageReuse(
			m_serverConfig.enablePageBufferReuse,
			m_serverConfig.pageBufferSize);
		if (m_packetCipher != nullptr && m_packetFramer == nullptr)
		{
			Log(GameServer::Foundation::ELogLevel::Error, "Packet cipher requires packet framer.");
			m_isRunning = false;
			return false;
		}

		m_sessionSlots = std::make_unique<std::atomic<FSession*>[]>(m_serverConfig.maxSessionCount);
		m_generations = std::make_unique<std::atomic<std::uint32_t>[]>(m_serverConfig.maxSessionCount);
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			m_sessionSlots[slotIndex].store(nullptr);
			m_generations[slotIndex].store(1);
		}

		if (!InitializeWinsock())
		{
			Log(GameServer::Foundation::ELogLevel::Error, "Winsock initialization failed.");
			Stop();
			return false;
		}

		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
		if (m_iocpHandle == nullptr)
		{
			std::ostringstream oss;
			oss << "CreateIoCompletionPort failed. error=" << GetLastError();
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			Stop();
			return false;
		}

		if (!OpenListenSocket())
		{
			Log(GameServer::Foundation::ELogLevel::Error, "Listen socket open failed.");
			Stop();
			return false;
		}

		StartWorkers();
		m_acceptThread = std::thread(&FIocpServer::AcceptLoop, this);
		{
			std::ostringstream oss;
			oss << "Server started. ip=" << m_serverConfig.bindIp
				<< " port=" << m_serverConfig.port
				<< " workers=" << m_serverConfig.workerThreadCount
				<< " maxSessions=" << m_serverConfig.maxSessionCount;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
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

		Log(GameServer::Foundation::ELogLevel::Info, "Server stop requested.");

		CloseListenSocket();

		if (m_acceptThread.joinable())
		{
			m_acceptThread.join();
		}

		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FSession* sessionContext = m_sessionSlots[slotIndex].exchange(nullptr);
			if (sessionContext != nullptr)
			{
				CloseSession(*sessionContext);
				ReleaseSession(sessionContext);
			}
		}

		StopWorkers();

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

		Log(GameServer::Foundation::ELogLevel::Info, "Server stopped.");
		m_packetCipher.reset();
		m_packetFramer.reset();
		m_logger.reset();
	}

	bool FIocpServer::Send(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length)
	{
		if ((buffer == nullptr && length > 0) || length < 0)
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "Send rejected because buffer is null or length is invalid.");
			return false;
		}

		FSession* sessionContext = AcquireSession(sessionId);
		if (sessionContext == nullptr)
		{
			std::ostringstream oss;
			oss << "Send rejected because session was not found. sessionId=" << sessionId;
			Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
			return false;
		}

		std::vector<char> framedBuffer;
		if (m_packetFramer != nullptr)
		{
			std::vector<char> payloadBuffer;
			payloadBuffer.resize(sizeof(GameServer::NetworkLib::Packet::SContentHeader) + static_cast<std::size_t>(length));
			GameServer::NetworkLib::Packet::SContentHeader contentHeader{};
			contentHeader.opcode = opcode;
			std::memcpy(payloadBuffer.data(), &contentHeader, sizeof(contentHeader));
			if (length > 0)
			{
				std::memcpy(
					payloadBuffer.data() + sizeof(GameServer::NetworkLib::Packet::SContentHeader),
					buffer,
					static_cast<std::size_t>(length));
			}
			std::uint8_t randomKey = 0;
			if (m_packetCipher != nullptr)
			{
				randomKey = GeneratePacketRandomKey();
				m_packetCipher->Encode(payloadBuffer.data(), static_cast<int>(payloadBuffer.size()), randomKey);
			}

			GameServer::NetworkLib::Packet::SOutgoingPacket outgoingPacket{};
			outgoingPacket.randomKey = randomKey;
			outgoingPacket.checkSum =
				GameServer::NetworkLib::Packet::CalculatePacketChecksum(
					payloadBuffer.data(),
					static_cast<std::int32_t>(payloadBuffer.size()));
			outgoingPacket.payload = payloadBuffer.data();
			outgoingPacket.payloadLength = static_cast<std::int32_t>(payloadBuffer.size());

			if (!m_packetFramer->BuildPacket(outgoingPacket, framedBuffer))
			{
				Log(GameServer::Foundation::ELogLevel::Error, "BuildPacket failed during send path.");
				ReleaseSession(sessionContext);
				return false;
			}
		}
		else
		{
			framedBuffer.assign(buffer, buffer + length);
		}

		sessionContext->EnqueueSendBuffer(FSendBuffer::Create(std::move(framedBuffer)));
		m_sentPacketCount.fetch_add(1, std::memory_order_relaxed);
		m_sentByteCount.fetch_add(static_cast<std::uint64_t>(length > 0 ? length : 0), std::memory_order_relaxed);
		PostSend(*sessionContext);

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
		stats.sessionPoolCapacity = static_cast<std::uint32_t>(FSession::GetPoolCapacity());
		stats.sessionPoolUsage = static_cast<std::uint32_t>(FSession::GetPoolUsage());
		stats.sendBufferPoolCapacity = static_cast<std::uint32_t>(FSendBuffer::GetPoolCapacity());
		stats.sendBufferPoolUsage = static_cast<std::uint32_t>(FSendBuffer::GetPoolUsage());
		stats.packetBufferPoolCapacity = static_cast<std::uint32_t>(GameServer::NetworkLib::Packet::FPacketBuffer::GetPoolCapacity());
		stats.packetBufferPoolUsage = static_cast<std::uint32_t>(GameServer::NetworkLib::Packet::FPacketBuffer::GetPoolUsage());

		std::uint64_t queuedSendBufferCount = 0;
		std::uint64_t maxObservedQueuedSendBufferCount = 0;
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FSession* sessionContext = m_sessionSlots[slotIndex].load(std::memory_order_relaxed);
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
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
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
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		sockaddr_in listenAddress{};
		listenAddress.sin_family = AF_INET;
		listenAddress.sin_port = htons(m_serverConfig.port);
		if (InetPtonA(AF_INET, m_serverConfig.bindIp.c_str(), &listenAddress.sin_addr) != 1)
		{
			std::ostringstream oss;
			oss << "InetPtonA failed for bind ip. ip=" << m_serverConfig.bindIp;
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		BOOL reuseAddress = TRUE;
		setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

		if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&listenAddress), sizeof(listenAddress)) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "bind failed. error=" << WSAGetLastError();
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "listen failed. error=" << WSAGetLastError();
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		return true;
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

	void FIocpServer::AcceptLoop()
	{
		while (m_isRunning)
		{
			SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
			if (clientSocket == INVALID_SOCKET)
			{
				if (m_isRunning)
				{
					std::ostringstream oss;
					oss << "accept failed. error=" << WSAGetLastError();
					Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				continue;
			}

			if (!AttachAcceptedSocket(clientSocket))
			{
				Log(GameServer::Foundation::ELogLevel::Warn, "Accepted socket was rejected because no session slot was available.");
				closesocket(clientSocket);
			}
		}
	}

	void FIocpServer::WorkerLoop()
	{
		while (true)
		{
			DWORD transferredBytes = 0;
			ULONG_PTR completionKey = 0;
			LPOVERLAPPED overlapped = nullptr;

			const BOOL queuedResult = GetQueuedCompletionStatus(m_iocpHandle, &transferredBytes, &completionKey, &overlapped, INFINITE);
			if (overlapped == nullptr && completionKey == 0)
			{
				break;
			}

			auto* ioContext = reinterpret_cast<FSession::SIoContext*>(overlapped);
			FSession* sessionContext = ioContext->ownerSession;
			if (sessionContext == nullptr)
			{
				continue;
			}

			if (queuedResult == FALSE || transferredBytes == 0)
			{
				if (queuedResult == FALSE)
				{
					std::ostringstream oss;
					oss << "I/O completion failed. sessionId=" << sessionContext->GetSessionId() << " error=" << GetLastError();
					Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
				}
				if (ioContext->ioType == FSession::EIoType::Send)
				{
					sessionContext->FinishSendIo();
					sessionContext->ReleaseActiveSendBuffers();
					sessionContext->EndSend();
				}
				CloseSession(*sessionContext);
				ReleaseSession(sessionContext);
				continue;
			}

			if (ioContext->ioType == FSession::EIoType::Recv)
			{
				m_receivedByteCount.fetch_add(transferredBytes, std::memory_order_relaxed);
				if (!sessionContext->CommitRecvBytes(transferredBytes))
				{
					Log(GameServer::Foundation::ELogLevel::Warn, "Recv buffer overflow detected.");
					CloseSession(*sessionContext);
					ReleaseSession(sessionContext);
					continue;
				}

				if (m_packetFramer != nullptr)
				{
					while (true)
					{
						GameServer::NetworkLib::Packet::FPacketView packetView;
						if (!m_packetFramer->TryExtractPacketView(sessionContext->GetRecvBuffer(), packetView))
						{
							break;
						}

						const std::uint8_t actualChecksum =
							GameServer::NetworkLib::Packet::CalculatePacketChecksum(
								packetView.payload,
								packetView.payloadLength);
						if (actualChecksum != packetView.checkSum)
						{
							std::ostringstream oss;
							oss << "Packet checksum mismatch. sessionId=" << sessionContext->GetSessionId()
								<< " opcode=" << packetView.opcode
								<< " expected=" << static_cast<int>(packetView.checkSum)
								<< " actual=" << static_cast<int>(actualChecksum);
							Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
							CloseSession(*sessionContext);
							break;
						}

					if (m_packetCipher != nullptr && packetView.payloadLength > 0)
					{
						m_packetCipher->Decode(const_cast<char*>(packetView.payload), packetView.payloadLength, packetView.randomKey);
					}

						GameServer::NetworkLib::Packet::FPacketView contentPacketView;
						if (!GameServer::NetworkLib::Packet::TryParseContentPacketView(packetView, contentPacketView))
						{
							std::ostringstream oss;
							oss << "Content header parse failed. sessionId=" << sessionContext->GetSessionId();
							Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
							CloseSession(*sessionContext);
							break;
						}

						m_applicationHandler->OnPacketReceived(
							*this,
							sessionContext->GetSessionId(),
							contentPacketView);
						m_receivedPacketCount.fetch_add(1, std::memory_order_relaxed);

						const std::size_t consumedPacketSize =
							sizeof(GameServer::NetworkLib::Packet::SPacketHeader) + static_cast<std::size_t>(packetView.payloadLength);
						sessionContext->GetRecvBuffer().Discard(consumedPacketSize);
					}
				}
				else
				{
					Log(GameServer::Foundation::ELogLevel::Warn, "Recv path without framer is not supported by ring buffer mode.");
					CloseSession(*sessionContext);
				}

				if (!PostRecv(*sessionContext))
				{
					std::ostringstream oss;
					oss << "PostRecv failed after packet dispatch. sessionId=" << sessionContext->GetSessionId();
					Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
					CloseSession(*sessionContext);
				}
			}
			else
			{
				sessionContext->FinishSendIo();
				sessionContext->ReleaseActiveSendBuffers();
				sessionContext->EndSend();
				if (!sessionContext->IsClosing())
				{
					PostSend(*sessionContext);
				}
			}

			ReleaseSession(sessionContext);
		}
	}

	bool FIocpServer::PostRecv(FSession& sessionContext)
	{
		DWORD recvFlags = 0;
		DWORD recvBytes = 0;
		WSABUF recvBuffers[2]{};
		DWORD recvBufferCount = 0;

		FSession::SIoContext& recvContext = sessionContext.GetRecvContext();
		recvContext.Prepare(FSession::EIoType::Recv, &sessionContext);
		sessionContext.BuildRecvWsabufs(recvBuffers, recvBufferCount);
		if (recvBufferCount == 0)
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "PostRecv failed because recv buffer has no writable space.");
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
			Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
			ReleaseSession(&sessionContext);
			return false;
		}

		return true;
	}

	bool FIocpServer::PostSend(FSession& sessionContext)
	{
		if (!sessionContext.TryBeginSend())
		{
			return false;
		}

		if (sessionContext.IsClosing())
		{
			sessionContext.EndSend();
			return false;
		}

		if (!sessionContext.FillSendBatch(kMaxSendBatchCount))
		{
			sessionContext.EndSend();
			return false;
		}

		FSession::SIoContext& sendContext = sessionContext.GetSendContext();
		sendContext.Prepare(FSession::EIoType::Send, &sessionContext);

		sessionContext.AcquireRef();
		const int concurrentSendIoCount = sessionContext.BeginSendIo();
		if (concurrentSendIoCount > 1)
		{
			std::ostringstream oss;
			oss << "Concurrent WSASend detected. sessionId=" << sessionContext.GetSessionId()
				<< " concurrentSendIoCount=" << concurrentSendIoCount;
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
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
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			sessionContext.FinishSendIo();
			sessionContext.ReleaseActiveSendBuffers();
			sessionContext.EndSend();
			ReleaseSession(&sessionContext);
			CloseSession(sessionContext);
			return false;
		}

		return true;
	}

	void FIocpServer::CloseSession(FSession& sessionContext)
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
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnClientDisconnected(sessionContext.GetSessionId());
	}

	void FIocpServer::ReleaseSession(FSession* sessionContext)
	{
		if (sessionContext == nullptr)
		{
			return;
		}

		if (sessionContext->ReleaseRef() == 0)
		{
			FSession::Destroy(sessionContext);
		}
	}

	FSession* FIocpServer::AcquireSession(std::uint64_t sessionId)
	{
		const std::uint32_t slotIndex = static_cast<std::uint32_t>(sessionId & 0xFFFFFFFFULL);
		if (slotIndex >= m_serverConfig.maxSessionCount)
		{
			return nullptr;
		}

		FSession* sessionContext = m_sessionSlots[slotIndex].load();
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
		FSession* newSessionContext = nullptr;

		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FSession* expected = nullptr;
			FSession* candidateSession = FSession::Create();
			const std::uint32_t generation = m_generations[slotIndex].fetch_add(1);
			const std::uint64_t sessionId = ComposeSessionId(slotIndex, generation);
			const std::size_t recvBufferCapacity =
				static_cast<std::size_t>(std::max<std::uint32_t>(m_serverConfig.recvBufferSize * 8u, 65536u));
			candidateSession->Initialize(clientSocket, sessionId, slotIndex, generation, recvBufferCapacity);

			if (m_sessionSlots[slotIndex].compare_exchange_strong(expected, candidateSession))
			{
				newSessionContext = candidateSession;
				break;
			}

			FSession::Destroy(candidateSession);
		}

		if (newSessionContext == nullptr)
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "All session slots are in use.");
			return false;
		}

		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), m_iocpHandle, 0, 0) == nullptr)
		{
			std::ostringstream oss;
			oss << "CreateIoCompletionPort attach failed. error=" << GetLastError();
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			m_sessionSlots[newSessionContext->GetSlotIndex()].store(nullptr);
			FSession::Destroy(newSessionContext);
			return false;
		}

		{
			std::ostringstream oss;
			oss << "Client connected. sessionId=" << newSessionContext->GetSessionId();
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
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

	void FIocpServer::Log(GameServer::Foundation::ELogLevel logLevel, const std::string& message) const
	{
		if (m_logger != nullptr)
		{
			m_logger->Log(logLevel, "NetworkLib", message);
		}
	}
}
