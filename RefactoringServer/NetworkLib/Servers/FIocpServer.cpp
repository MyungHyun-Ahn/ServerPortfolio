#include "Pch.h"

#include "Crypto/IPacketCipher.h"
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
		if (m_packetCipher != nullptr && m_packetFramer == nullptr)
		{
			Log(GameServer::Foundation::ELogLevel::Error, "Packet cipher requires packet framer.");
			m_isRunning = false;
			return false;
		}

		m_sessionSlots = std::make_unique<std::atomic<SSessionContext*>[]>(m_serverConfig.maxSessionCount);
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
			SSessionContext* sessionContext = m_sessionSlots[slotIndex].exchange(nullptr);
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
		if (buffer == nullptr || length <= 0)
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "Send rejected because buffer is null or length is invalid.");
			return false;
		}

		SSessionContext* sessionContext = AcquireSession(sessionId);
		if (sessionContext == nullptr)
		{
			std::ostringstream oss;
			oss << "Send rejected because session was not found. sessionId=" << sessionId;
			Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
			return false;
		}

		auto* ioContext = new SIoContext();
		ioContext->ioType = EIoType::Send;
		ioContext->ownerSession = sessionContext;
		if (m_packetFramer != nullptr)
		{
			std::vector<char> payloadBuffer(buffer, buffer + length);
			std::uint8_t randomKey = 0;
			if (m_packetCipher != nullptr)
			{
				randomKey = GeneratePacketRandomKey();
				m_packetCipher->Encode(payloadBuffer.data(), static_cast<int>(payloadBuffer.size()), randomKey);
			}

			GameServer::NetworkLib::Packet::SOutgoingPacket outgoingPacket{};
			outgoingPacket.opcode = opcode;
			outgoingPacket.randomKey = randomKey;
			outgoingPacket.checkSum =
				GameServer::NetworkLib::Packet::CalculatePacketChecksum(
					payloadBuffer.data(),
					static_cast<std::int32_t>(payloadBuffer.size()));
			outgoingPacket.payload = payloadBuffer.data();
			outgoingPacket.payloadLength = static_cast<std::int32_t>(payloadBuffer.size());

			if (!m_packetFramer->BuildPacket(outgoingPacket, ioContext->buffer))
			{
				Log(GameServer::Foundation::ELogLevel::Error, "BuildPacket failed during send path.");
				delete ioContext;
				ReleaseSession(sessionContext);
				return false;
			}
		}
		else
		{
			ioContext->buffer.assign(buffer, buffer + length);
		}

		ioContext->wsabuf.buf = ioContext->buffer.data();
		ioContext->wsabuf.len = static_cast<ULONG>(ioContext->buffer.size());
		sessionContext->refCount.fetch_add(1);

		DWORD sentBytes = 0;
		const int sendResult = WSASend(sessionContext->socket, &ioContext->wsabuf, 1, &sentBytes, 0, &ioContext->overlapped, nullptr);
		if (sendResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "WSASend failed. sessionId=" << sessionId << " error=" << errorCode;
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());
			CloseSession(*sessionContext);
			delete ioContext;
			ReleaseSession(sessionContext);
			ReleaseSession(sessionContext);
			return false;
		}

		ReleaseSession(sessionContext);
		return true;
	}

	EBackendKind FIocpServer::GetBackendKind() const
	{
		return EBackendKind::Iocp;
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

			auto* ioContext = reinterpret_cast<SIoContext*>(overlapped);
			SSessionContext* sessionContext = ioContext->ownerSession;
			if (sessionContext == nullptr)
			{
				if (ioContext->ioType == EIoType::Send)
				{
					delete ioContext;
				}
				continue;
			}

			if (queuedResult == FALSE || transferredBytes == 0)
			{
				if (queuedResult == FALSE)
				{
					std::ostringstream oss;
					oss << "I/O completion failed. sessionId=" << sessionContext->sessionId << " error=" << GetLastError();
					Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
				}
				CloseSession(*sessionContext);
				if (ioContext->ioType == EIoType::Send)
				{
					delete ioContext;
				}
				ReleaseSession(sessionContext);
				continue;
			}

			if (ioContext->ioType == EIoType::Recv)
			{
				sessionContext->recvBuffer.insert(
					sessionContext->recvBuffer.end(),
					ioContext->buffer.data(),
					ioContext->buffer.data() + transferredBytes);

				if (m_packetFramer != nullptr)
				{
					while (true)
					{
						GameServer::NetworkLib::Packet::SFramedPacket framedPacket;
						if (!m_packetFramer->TryExtractPacket(sessionContext->recvBuffer, framedPacket))
						{
							break;
						}

						const std::uint8_t actualChecksum =
							GameServer::NetworkLib::Packet::CalculatePacketChecksum(
								framedPacket.payload.data(),
								static_cast<std::int32_t>(framedPacket.payload.size()));
						if (actualChecksum != framedPacket.checkSum)
						{
							std::ostringstream oss;
							oss << "Packet checksum mismatch. sessionId=" << sessionContext->sessionId
								<< " opcode=" << framedPacket.opcode
								<< " expected=" << static_cast<int>(framedPacket.checkSum)
								<< " actual=" << static_cast<int>(actualChecksum);
							Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
							CloseSession(*sessionContext);
							break;
						}

						if (m_packetCipher != nullptr && !framedPacket.payload.empty())
						{
							m_packetCipher->Decode(framedPacket.payload.data(), static_cast<int>(framedPacket.payload.size()), framedPacket.randomKey);
						}

						m_applicationHandler->OnPacketReceived(
							*this,
							sessionContext->sessionId,
							framedPacket.opcode,
							framedPacket.payload.data(),
							static_cast<std::int32_t>(framedPacket.payload.size()));
					}
				}
				else
				{
					m_applicationHandler->OnPacketReceived(
						*this,
						sessionContext->sessionId,
						0,
						ioContext->buffer.data(),
						static_cast<std::int32_t>(transferredBytes));
				}

				if (!PostRecv(*sessionContext))
				{
					std::ostringstream oss;
					oss << "PostRecv failed after packet dispatch. sessionId=" << sessionContext->sessionId;
					Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
					CloseSession(*sessionContext);
				}
			}
			else
			{
				delete ioContext;
			}

			ReleaseSession(sessionContext);
		}
	}

	bool FIocpServer::PostRecv(SSessionContext& sessionContext)
	{
		DWORD recvFlags = 0;
		DWORD recvBytes = 0;

		ZeroMemory(&sessionContext.recvContext.overlapped, sizeof(sessionContext.recvContext.overlapped));
		sessionContext.recvContext.ioType = EIoType::Recv;
		sessionContext.recvContext.ownerSession = &sessionContext;
		sessionContext.recvContext.buffer.resize(m_serverConfig.recvBufferSize);
		sessionContext.recvContext.wsabuf.buf = sessionContext.recvContext.buffer.data();
		sessionContext.recvContext.wsabuf.len = static_cast<ULONG>(sessionContext.recvContext.buffer.size());
		sessionContext.refCount.fetch_add(1);

		const int recvResult = WSARecv(sessionContext.socket, &sessionContext.recvContext.wsabuf, 1, &recvBytes, &recvFlags, &sessionContext.recvContext.overlapped, nullptr);
		if (recvResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "WSARecv failed. sessionId=" << sessionContext.sessionId << " error=" << errorCode;
			Log(GameServer::Foundation::ELogLevel::Warn, oss.str());
			ReleaseSession(&sessionContext);
			return false;
		}

		return true;
	}

	void FIocpServer::CloseSession(SSessionContext& sessionContext)
	{
		bool expected = false;
		if (!sessionContext.closing.compare_exchange_strong(expected, true))
		{
			return;
		}

		shutdown(sessionContext.socket, SD_BOTH);
		closesocket(sessionContext.socket);
		sessionContext.socket = INVALID_SOCKET;
		m_sessionSlots[sessionContext.slotIndex].store(nullptr);
		{
			std::ostringstream oss;
			oss << "Session closed. sessionId=" << sessionContext.sessionId;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnClientDisconnected(sessionContext.sessionId);
	}

	void FIocpServer::ReleaseSession(SSessionContext* sessionContext)
	{
		if (sessionContext == nullptr)
		{
			return;
		}

		if (sessionContext->refCount.fetch_sub(1) == 1)
		{
			delete sessionContext;
		}
	}

	FIocpServer::SSessionContext* FIocpServer::AcquireSession(std::uint64_t sessionId)
	{
		const std::uint32_t slotIndex = static_cast<std::uint32_t>(sessionId & 0xFFFFFFFFULL);
		if (slotIndex >= m_serverConfig.maxSessionCount)
		{
			return nullptr;
		}

		SSessionContext* sessionContext = m_sessionSlots[slotIndex].load();
		if (sessionContext == nullptr || sessionContext->sessionId != sessionId || sessionContext->closing.load())
		{
			return nullptr;
		}

		sessionContext->refCount.fetch_add(1);
		if (sessionContext->sessionId != sessionId || sessionContext->closing.load())
		{
			ReleaseSession(sessionContext);
			return nullptr;
		}

		return sessionContext;
	}

	bool FIocpServer::AttachAcceptedSocket(SOCKET clientSocket)
	{
		SSessionContext* newSessionContext = nullptr;

		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			SSessionContext* expected = nullptr;
			auto* candidateSession = new SSessionContext();
			candidateSession->socket = clientSocket;
			candidateSession->slotIndex = slotIndex;
			candidateSession->generation = m_generations[slotIndex].fetch_add(1);
			candidateSession->sessionId = ComposeSessionId(slotIndex, candidateSession->generation);

			if (m_sessionSlots[slotIndex].compare_exchange_strong(expected, candidateSession))
			{
				newSessionContext = candidateSession;
				break;
			}

			delete candidateSession;
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
			m_sessionSlots[newSessionContext->slotIndex].store(nullptr);
			delete newSessionContext;
			return false;
		}

		{
			std::ostringstream oss;
			oss << "Client connected. sessionId=" << newSessionContext->sessionId;
			Log(GameServer::Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnClientConnected(newSessionContext->sessionId);
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
