#include "Pch.h"

#include "Crypto/IPacketCipher.h"
#include "Packet/Buffer/FPacketBuffer.h"
#include "Packet/Buffer/FSendBuffer.h"
#include "Packet/Framing/IPacketFramer.h"
#include "Packet/Serialization/FPacketSerialization.h"
#include "Servers/Core/BackendTypes.h"
#include "Servers/Core/FRioServer.h"
#include "Servers/IApplicationHandler.h"
#include "Servers/Session/FRioSession.h"
#include "Foundation/Logging/ILogger.h"

#pragma comment(lib, "Ws2_32.lib")

namespace NetworkLib::Core
{
	using NetworkLib::Packet::Buffer::FPacketBuffer;
	using NetworkLib::Packet::Buffer::FSendBuffer;
	using NetworkLib::Packet::Framing::CalculatePacketChecksum;
	using NetworkLib::Packet::Framing::SPacketHeader;
	using NetworkLib::Packet::Framing::SOutgoingPacket;
	using NetworkLib::Packet::Serialization::TryParseContentPacketView;
	using NetworkLib::Packet::View::FPacketView;
	using NetworkLib::Session::FRioSession;

	namespace
	{
		inline constexpr std::size_t kCompletionBatchSize = 128;
		inline constexpr DWORD kWorkerWaitTimeoutMs = 100;
		inline constexpr ULONG kMaxOutstandingReceive = 1;
		inline constexpr ULONG kMaxReceiveDataBuffers = 1;
		inline constexpr ULONG kMaxOutstandingSend = 8;
		inline constexpr ULONG kMaxSendDataBuffers = 1;

		bool ApplyAcceptedSocketSendBufferOption(
			const SServerConfig& serverConfig,
			const SOCKET clientSocket,
			std::string& outError) noexcept
		{
			const int sendBufferBytes =
				serverConfig.socketSendBufferBytes < 0
				? 0
				: serverConfig.socketSendBufferBytes;
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

	FRioServer::FRioServer()
	{
		m_rioFunctionTable.cbSize = sizeof(m_rioFunctionTable);
	}

	FRioServer::~FRioServer()
	{
		Stop();
	}

	bool FRioServer::Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler)
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
			Stop();
			return false;
		}

		m_sessionSlots = std::make_unique<std::atomic<FRioSession*>[]>(m_serverConfig.maxSessionCount);
		m_generations = std::make_unique<std::atomic<std::uint32_t>[]>(m_serverConfig.maxSessionCount);
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			m_sessionSlots[slotIndex].store(nullptr);
			m_generations[slotIndex].store(1);
		}

		FRioSession::EnsurePoolCapacity(static_cast<LONG>(m_serverConfig.maxSessionCount));

		if (!InitializeWinsock())
		{
			Log(Foundation::ELogLevel::Error, "Winsock initialization failed.");
			Stop();
			return false;
		}

		if (!LoadRioFunctionTable())
		{
			Log(Foundation::ELogLevel::Error, "RIO function table load failed.");
			Stop();
			return false;
		}

		if (!FSendBuffer::InitializeSegmentPool(true, &m_rioFunctionTable, m_serverConfig.maxSessionCount))
		{
			Log(Foundation::ELogLevel::Error, "RIO send segment pool initialization failed.");
			Stop();
			return false;
		}

		if (!OpenListenSocket())
		{
			Log(Foundation::ELogLevel::Error, "Listen socket open failed.");
			Stop();
			return false;
		}

		if (!LoadAcceptExFunction())
		{
			Log(Foundation::ELogLevel::Error, "AcceptEx function load failed.");
			Stop();
			return false;
		}

		if (!StartWorkers())
		{
			Log(Foundation::ELogLevel::Error, "RIO worker startup failed.");
			Stop();
			return false;
		}

		m_acceptThread = std::thread(&FRioServer::AcceptLoop, this);
		{
			std::ostringstream oss;
			oss << "RIO server started. ip=" << m_serverConfig.bindIp
				<< " port=" << m_serverConfig.port
				<< " workers=" << m_workers.size()
				<< " maxSessions=" << m_serverConfig.maxSessionCount;
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnServerStarted(*this);
		return true;
	}

	void FRioServer::Stop()
	{
		if (!m_isRunning.exchange(false))
		{
			return;
		}

		Log(Foundation::ELogLevel::Info, "RIO server stop requested.");

		CloseListenSocket();

		if (m_acceptThread.joinable())
		{
			m_acceptThread.join();
		}

		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FRioSession* sessionContext = m_sessionSlots[slotIndex].exchange(nullptr);
			if (sessionContext != nullptr)
			{
				CloseSession(*sessionContext);
				ReleaseSession(sessionContext);
			}
		}

		StopWorkers();
		FSendBuffer::ShutdownSegmentPool(&m_rioFunctionTable);

		if (m_winsockInitialized.exchange(false))
		{
			WSACleanup();
		}

		if (m_applicationHandler != nullptr)
		{
			m_applicationHandler->OnServerStopped();
			m_applicationHandler = nullptr;
		}

		Log(Foundation::ELogLevel::Info, "RIO server stopped.");
		m_packetCipher.reset();
		m_packetFramer.reset();
		m_acceptEx = nullptr;
		m_logger.reset();
	}

	bool FRioServer::SendPacket(
		std::uint64_t sessionId,
		NetworkLib::Packet::Serialization::FOutgoingContentPacket&& packet)
	{
		if (!packet.IsValid())
		{
			Log(Foundation::ELogLevel::Warn, "Send rejected because outgoing packet was invalid.");
			return false;
		}

		const std::int32_t bodyLength = packet.GetBodyLength();
		FRioSession* sessionContext = AcquireSession(sessionId);
		if (sessionContext == nullptr)
		{
			std::ostringstream oss;
			oss << "Send rejected because session was not found. sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Warn, oss.str());
			return false;
		}

		FPacketBuffer* packetBuffer = packet.ReleaseBuffer();
		if (m_packetFramer != nullptr)
		{
			std::vector<char>& payloadBuffer = packetBuffer->GetBuffer();

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

			FPacketBuffer* framedPacketBuffer = FPacketBuffer::Create();
			std::vector<char>& framedBuffer = framedPacketBuffer->GetBuffer();
			if (!m_packetFramer->BuildPacket(outgoingPacket, framedBuffer))
			{
				Log(Foundation::ELogLevel::Error, "BuildPacket failed during RIO send path.");
				FPacketBuffer::Release(packetBuffer);
				FPacketBuffer::Release(framedPacketBuffer);
				ReleaseSession(sessionContext);
				return false;
			}

			FPacketBuffer::Release(packetBuffer);
			packetBuffer = framedPacketBuffer;
		}

		if (packetBuffer == nullptr || packetBuffer->GetBuffer().empty())
		{
			Log(Foundation::ELogLevel::Warn, "RIO send rejected because framed buffer is empty.");
			FPacketBuffer::Release(packetBuffer);
			ReleaseSession(sessionContext);
			return false;
		}

		FSendBuffer* sendBuffer = FSendBuffer::Create(packetBuffer);
		if (sendBuffer == nullptr || sendBuffer->GetSize() == 0)
		{
			Log(Foundation::ELogLevel::Warn, "RIO send rejected because send buffer creation failed.");
			FSendBuffer::Release(sendBuffer);
			ReleaseSession(sessionContext);
			return false;
		}

		const bool sendResult =
			m_serverConfig.rioSendDispatchMode == ERioSendDispatchMode::OwnerThread
				? EnqueueOwnerThreadSend(
					sessionId,
					sessionContext->GetOwnerWorkerIndex(),
					sendBuffer,
					bodyLength)
				: SubmitSendDirect(*sessionContext, sessionId, sendBuffer, bodyLength);
		ReleaseSession(sessionContext);
		return sendResult;
	}

	bool FRioServer::Disconnect(std::uint64_t sessionId)
	{
		FRioSession* sessionContext = AcquireSession(sessionId);
		if (sessionContext == nullptr)
		{
			return false;
		}

		CloseSession(*sessionContext);
		ReleaseSession(sessionContext);
		return true;
	}

	EBackendKind FRioServer::GetBackendKind() const
	{
		return EBackendKind::Rio;
	}

	SServerStats FRioServer::GetStatsSnapshot() const
	{
		SServerStats stats{};
		stats.activeSessionCount = m_activeSessionCount.load(std::memory_order_relaxed);
		stats.acceptedSessionCount = m_acceptedSessionCount.load(std::memory_order_relaxed);
		stats.receivedPacketCount = m_receivedPacketCount.load(std::memory_order_relaxed);
		stats.sentPacketCount = m_sentPacketCount.load(std::memory_order_relaxed);
		stats.receivedByteCount = m_receivedByteCount.load(std::memory_order_relaxed);
		stats.sentByteCount = m_sentByteCount.load(std::memory_order_relaxed);
		stats.sessionPoolCapacity = static_cast<std::uint32_t>(FRioSession::GetPoolCapacity());
		stats.sessionPoolUsage = static_cast<std::uint32_t>(FRioSession::GetPoolUsage());
		stats.sendBufferPoolCapacity = static_cast<std::uint32_t>(FSendBuffer::GetPoolCapacity());
		stats.sendBufferPoolUsage = static_cast<std::uint32_t>(FSendBuffer::GetPoolUsage());
		stats.packetBufferPoolCapacity = static_cast<std::uint32_t>(FPacketBuffer::GetPoolCapacity());
		stats.packetBufferPoolUsage = static_cast<std::uint32_t>(FPacketBuffer::GetPoolUsage());

		std::uint64_t queuedSendBufferCount = 0;
		std::uint64_t maxObservedQueuedSendBufferCount = 0;
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			FRioSession* sessionContext = m_sessionSlots[slotIndex].load(std::memory_order_relaxed);
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

	bool FRioServer::InitializeWinsock()
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

	bool FRioServer::LoadRioFunctionTable()
	{
		SOCKET tempSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO);
		if (tempSocket == INVALID_SOCKET)
		{
			std::ostringstream oss;
			oss << "WSASocketW for RIO function table failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		GUID rioGuid = WSAID_MULTIPLE_RIO;
		DWORD bytesReturned = 0;
		m_rioFunctionTable = {};
		m_rioFunctionTable.cbSize = sizeof(m_rioFunctionTable);
		const int ioctlResult = WSAIoctl(
			tempSocket,
			SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
			&rioGuid,
			sizeof(rioGuid),
			&m_rioFunctionTable,
			sizeof(m_rioFunctionTable),
			&bytesReturned,
			nullptr,
			nullptr);
		closesocket(tempSocket);
		if (ioctlResult == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER) failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		return true;
	}

	bool FRioServer::OpenListenSocket()
	{
		m_listenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO);
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
		setsockopt(
			m_listenSocket,
			SOL_SOCKET,
			SO_REUSEADDR,
			reinterpret_cast<const char*>(&reuseAddress),
			sizeof(reuseAddress));

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

	bool FRioServer::LoadAcceptExFunction()
	{
		GUID acceptExGuid = WSAID_ACCEPTEX;
		DWORD bytesReturned = 0;
		const int ioctlResult = WSAIoctl(
			m_listenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&acceptExGuid,
			sizeof(acceptExGuid),
			&m_acceptEx,
			sizeof(m_acceptEx),
			&bytesReturned,
			nullptr,
			nullptr);
		if (ioctlResult == SOCKET_ERROR)
		{
			std::ostringstream oss;
			oss << "WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER, AcceptEx) failed. error=" << WSAGetLastError();
			Log(Foundation::ELogLevel::Error, oss.str());
			return false;
		}

		return m_acceptEx != nullptr;
	}

	void FRioServer::CloseListenSocket()
	{
		if (m_listenSocket != INVALID_SOCKET)
		{
			closesocket(m_listenSocket);
			m_listenSocket = INVALID_SOCKET;
		}
	}

	bool FRioServer::StartWorkers()
	{
		const std::uint32_t workerCount = std::max(1u, m_serverConfig.workerThreadCount);
		m_workers.clear();
		m_workers.reserve(workerCount);
		const DWORD completionQueueSize =
			std::max<DWORD>(256, static_cast<DWORD>(std::max(1u, m_serverConfig.maxSessionCount) * 8u));

		for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
		{
			auto worker = std::make_unique<SRioWorker>();
			worker->completionEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
			if (worker->completionEvent == nullptr)
			{
				std::ostringstream oss;
				oss << "CreateEvent failed for RIO worker. workerIndex=" << workerIndex
					<< " error=" << GetLastError();
				Log(Foundation::ELogLevel::Error, oss.str());
				return false;
			}

			RIO_NOTIFICATION_COMPLETION notificationCompletion{};
			notificationCompletion.Type = RIO_EVENT_COMPLETION;
			notificationCompletion.Event.EventHandle = worker->completionEvent;
			notificationCompletion.Event.NotifyReset = TRUE;
			worker->completionQueue =
				m_rioFunctionTable.RIOCreateCompletionQueue(completionQueueSize, &notificationCompletion);
			if (worker->completionQueue == RIO_INVALID_CQ)
			{
				std::ostringstream oss;
				oss << "RIOCreateCompletionQueue failed. workerIndex=" << workerIndex
					<< " error=" << WSAGetLastError();
				Log(Foundation::ELogLevel::Error, oss.str());
				return false;
			}

			m_workers.push_back(std::move(worker));
		}

		for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
		{
			m_workers[workerIndex]->thread = std::thread(&FRioServer::WorkerLoop, this, workerIndex);
		}

		return true;
	}

	void FRioServer::StopWorkers()
	{
		const auto waitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (FRioSession::GetPoolUsage() > 0 && std::chrono::steady_clock::now() < waitDeadline)
		{
			for (const auto& worker : m_workers)
			{
				if (worker != nullptr && worker->completionEvent != nullptr)
				{
					SetEvent(worker->completionEvent);
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		for (const auto& worker : m_workers)
		{
			if (worker != nullptr && worker->completionEvent != nullptr)
			{
				SetEvent(worker->completionEvent);
			}
		}

		for (const auto& worker : m_workers)
		{
			if (worker != nullptr && worker->thread.joinable())
			{
				worker->thread.join();
			}
		}

		for (const auto& worker : m_workers)
		{
			if (worker == nullptr)
			{
				continue;
			}

			std::deque<SSendCommand> pendingCommands;
			{
				std::scoped_lock<std::mutex> sendCommandLock(worker->sendCommandMutex);
				pendingCommands.swap(worker->sendCommands);
			}

			for (SSendCommand& command : pendingCommands)
			{
				if (command.sendBuffer != nullptr)
				{
					FSendBuffer::Release(command.sendBuffer);
					command.sendBuffer = nullptr;
				}
			}
		}

		for (const auto& worker : m_workers)
		{
			if (worker == nullptr)
			{
				continue;
			}

			if (worker->completionQueue != RIO_INVALID_CQ)
			{
				m_rioFunctionTable.RIOCloseCompletionQueue(worker->completionQueue);
			}

			if (worker->completionEvent != nullptr)
			{
				CloseHandle(worker->completionEvent);
			}
		}

		m_workers.clear();
	}

	void FRioServer::AcceptLoop()
	{
		while (m_isRunning.load(std::memory_order_acquire))
		{
			SOCKET clientSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_REGISTERED_IO);
			if (clientSocket == INVALID_SOCKET)
			{
				std::ostringstream oss;
				oss << "WSASocketW failed for accept socket. error=" << WSAGetLastError();
				Log(Foundation::ELogLevel::Warn, oss.str());
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			std::array<char, (sizeof(sockaddr_storage) + 16) * 2> addressBuffer{};
			WSAOVERLAPPED acceptOverlapped{};
			acceptOverlapped.hEvent = WSACreateEvent();
			if (acceptOverlapped.hEvent == WSA_INVALID_EVENT)
			{
				std::ostringstream oss;
				oss << "WSACreateEvent failed for AcceptEx. error=" << WSAGetLastError();
				Log(Foundation::ELogLevel::Warn, oss.str());
				closesocket(clientSocket);
				continue;
			}

			DWORD bytesReceived = 0;
			BOOL acceptResult = m_acceptEx(
				m_listenSocket,
				clientSocket,
				addressBuffer.data(),
				0,
				sizeof(sockaddr_storage) + 16,
				sizeof(sockaddr_storage) + 16,
				&bytesReceived,
				&acceptOverlapped);
			if (acceptResult == FALSE)
			{
				const int errorCode = WSAGetLastError();
				if (errorCode != ERROR_IO_PENDING)
				{
					std::ostringstream oss;
					oss << "AcceptEx failed. error=" << errorCode;
					Log(Foundation::ELogLevel::Warn, oss.str());
					WSACloseEvent(acceptOverlapped.hEvent);
					closesocket(clientSocket);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				DWORD transferredBytes = 0;
				DWORD completionFlags = 0;
				bool waitSucceeded = false;
				while (m_isRunning.load(std::memory_order_acquire))
				{
					const DWORD waitResult = WSAWaitForMultipleEvents(1, &acceptOverlapped.hEvent, TRUE, 100, FALSE);
					if (waitResult == WSA_WAIT_TIMEOUT)
					{
						continue;
					}

					if (waitResult == WSA_WAIT_EVENT_0)
					{
						waitSucceeded = WSAGetOverlappedResult(
							m_listenSocket,
							&acceptOverlapped,
							&transferredBytes,
							FALSE,
							&completionFlags) == TRUE;
						break;
					}

					std::ostringstream oss;
					oss << "WSAWaitForMultipleEvents failed for AcceptEx. result=" << waitResult
						<< " error=" << WSAGetLastError();
					Log(Foundation::ELogLevel::Warn, oss.str());
					break;
				}

				if (!m_isRunning.load(std::memory_order_acquire))
				{
					WSACloseEvent(acceptOverlapped.hEvent);
					closesocket(clientSocket);
					break;
				}

				if (!waitSucceeded)
				{
					WSACloseEvent(acceptOverlapped.hEvent);
					closesocket(clientSocket);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}
			}

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
				WSACloseEvent(acceptOverlapped.hEvent);
				closesocket(clientSocket);
				continue;
			}

			WSACloseEvent(acceptOverlapped.hEvent);
			if (!AttachAcceptedSocket(clientSocket))
			{
				Log(Foundation::ELogLevel::Warn, "Accepted socket was rejected because no session slot was available.");
				closesocket(clientSocket);
			}
		}
	}

	void FRioServer::WorkerLoop(std::uint32_t workerIndex)
	{
		SRioWorker& worker = *m_workers[workerIndex];
		std::array<RIORESULT, kCompletionBatchSize> completionResults{};
		bool notificationArmed = false;

		try
		{
			while (true)
			{
				DrainSendCommands(workerIndex);
				if (!notificationArmed)
				{
					if (m_rioFunctionTable.RIONotify(worker.completionQueue) != SOCKET_ERROR)
					{
						notificationArmed = true;
					}
				}

				const DWORD waitResult = WaitForSingleObject(worker.completionEvent, kWorkerWaitTimeoutMs);
				if (waitResult == WAIT_OBJECT_0)
				{
					notificationArmed = false;
					while (true)
					{
						const ULONG completionCount =
							m_rioFunctionTable.RIODequeueCompletion(
								worker.completionQueue,
								completionResults.data(),
								static_cast<ULONG>(completionResults.size()));
						if (completionCount == 0)
						{
							break;
						}

						for (ULONG completionIndex = 0; completionIndex < completionCount; ++completionIndex)
						{
							HandleRioCompletion(completionResults[completionIndex]);
						}
					}
				}
				else if (waitResult != WAIT_TIMEOUT)
				{
					std::ostringstream oss;
					oss << "WaitForSingleObject failed in RIO worker. workerIndex=" << workerIndex
						<< " error=" << GetLastError();
					Log(Foundation::ELogLevel::Warn, oss.str());
				}

				DrainSendCommands(workerIndex);
				if (!m_isRunning.load(std::memory_order_acquire) &&
					m_activeSessionCount.load(std::memory_order_acquire) == 0 &&
					FRioSession::GetPoolUsage() == 0 &&
					!HasPendingSendCommands(workerIndex))
				{
					break;
				}
			}
		}
		catch (const std::exception& exception)
		{
			std::ostringstream oss;
			oss << "Unhandled std::exception in RIO worker loop. workerIndex=" << workerIndex
				<< " message=" << exception.what()
				<< " maxQueuedSendCommands=" << worker.maxObservedSendCommandCount.load(std::memory_order_relaxed);
			Log(Foundation::ELogLevel::Error, oss.str());
			throw;
		}
		catch (...)
		{
			std::ostringstream oss;
			oss << "Unhandled unknown exception in RIO worker loop. workerIndex=" << workerIndex
				<< " maxQueuedSendCommands=" << worker.maxObservedSendCommandCount.load(std::memory_order_relaxed);
			Log(Foundation::ELogLevel::Error, oss.str());
			throw;
		}
	}

	void FRioServer::DrainSendCommands(std::uint32_t workerIndex)
	{
		if (workerIndex >= m_workers.size())
		{
			return;
		}

		std::deque<SSendCommand> pendingCommands;
		{
			std::scoped_lock<std::mutex> sendCommandLock(m_workers[workerIndex]->sendCommandMutex);
			pendingCommands.swap(m_workers[workerIndex]->sendCommands);
		}

		for (SSendCommand& command : pendingCommands)
		{
			if (command.sendBuffer == nullptr)
			{
				continue;
			}

			FRioSession* sessionContext = AcquireSession(command.sessionId);
			if (sessionContext == nullptr)
			{
				FSendBuffer::Release(command.sendBuffer);
				command.sendBuffer = nullptr;
				continue;
			}

			if (sessionContext->GetOwnerWorkerIndex() != workerIndex)
			{
				std::ostringstream oss;
				oss << "RIO owner-thread send command rejected because worker ownership mismatched. sessionId="
					<< command.sessionId
					<< " expectedWorkerIndex=" << sessionContext->GetOwnerWorkerIndex()
					<< " actualWorkerIndex=" << workerIndex;
				Log(Foundation::ELogLevel::Warn, oss.str());
				FSendBuffer::Release(command.sendBuffer);
				command.sendBuffer = nullptr;
				ReleaseSession(sessionContext);
				continue;
			}

			SubmitSendDirect(*sessionContext, command.sessionId, command.sendBuffer, command.payloadLength);
			command.sendBuffer = nullptr;
			ReleaseSession(sessionContext);
		}
	}

	bool FRioServer::SubmitSendDirect(
		FRioSession& sessionContext,
		const std::uint64_t sessionId,
		FSendBuffer* sendBuffer,
		const std::int32_t payloadLength)
	{
		if (sendBuffer == nullptr || sendBuffer->GetSize() == 0)
		{
			Log(Foundation::ELogLevel::Warn, "RIO direct send rejected because send buffer was invalid.");
			return false;
		}

		auto* sendRequestContext = new FRioSession::SSendRequestContext{};
		sendRequestContext->requestKind = FRioSession::ERequestKind::Send;
		sendRequestContext->ownerSession = &sessionContext;
		sendRequestContext->sendBuffer = sendBuffer;
		if (!sendBuffer->TryBuildRioBuf(sendRequestContext->buffer))
		{
			sendRequestContext->ownsBufferRegistration = true;
			sendRequestContext->bufferId =
				m_rioFunctionTable.RIORegisterBuffer(
					sendBuffer->GetData(),
					static_cast<DWORD>(sendBuffer->GetSize()));
			if (sendRequestContext->bufferId == RIO_INVALID_BUFFERID)
			{
				const int errorCode = WSAGetLastError();
				std::ostringstream oss;
				oss << "RIORegisterBuffer failed during send path. sessionId=" << sessionId
					<< " error=" << errorCode;
				Log(Foundation::ELogLevel::Error, oss.str());
				FSendBuffer::Release(sendBuffer);
				delete sendRequestContext;
				return false;
			}

			sendRequestContext->buffer.BufferId = sendRequestContext->bufferId;
			sendRequestContext->buffer.Offset = 0;
			sendRequestContext->buffer.Length = static_cast<ULONG>(sendBuffer->GetSize());
		}

		bool sendResult = false;
		sessionContext.OnSendQueued();
		{
			std::scoped_lock<std::mutex> requestQueueLock(sessionContext.GetRequestQueueMutex());
			if (!sessionContext.IsClosing() && sessionContext.GetRequestQueue() != RIO_INVALID_RQ)
			{
				sessionContext.AcquireRef();
				sendResult = m_rioFunctionTable.RIOSend(
					sessionContext.GetRequestQueue(),
					&sendRequestContext->buffer,
					1,
					0,
					sendRequestContext) == TRUE;
				if (!sendResult)
				{
					ReleaseSession(&sessionContext);
				}
			}
		}

		if (!sendResult)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "RIOSend failed. sessionId=" << sessionId << " error=" << errorCode;
			Log(Foundation::ELogLevel::Warn, oss.str());
			sessionContext.OnSendCompleted();
			if (sendRequestContext->ownsBufferRegistration &&
				sendRequestContext->bufferId != RIO_INVALID_BUFFERID)
			{
				m_rioFunctionTable.RIODeregisterBuffer(sendRequestContext->bufferId);
			}
			FSendBuffer::Release(sendBuffer);
			delete sendRequestContext;
			if (!sessionContext.IsClosing())
			{
				CloseSession(sessionContext);
			}
			return false;
		}

		m_sentPacketCount.fetch_add(1, std::memory_order_relaxed);
		m_sentByteCount.fetch_add(static_cast<std::uint64_t>(payloadLength > 0 ? payloadLength : 0), std::memory_order_relaxed);
		return true;
	}

	bool FRioServer::EnqueueOwnerThreadSend(
		const std::uint64_t sessionId,
		const std::uint32_t ownerWorkerIndex,
		FSendBuffer* sendBuffer,
		const std::int32_t payloadLength)
	{
		if (sendBuffer == nullptr || sendBuffer->GetSize() == 0)
		{
			Log(Foundation::ELogLevel::Warn, "RIO owner-thread send rejected because send buffer was invalid.");
			return false;
		}

		if (ownerWorkerIndex >= m_workers.size())
		{
			std::ostringstream oss;
			oss << "RIO owner-thread send rejected because worker index was invalid. sessionId=" << sessionId
				<< " workerIndex=" << ownerWorkerIndex;
			Log(Foundation::ELogLevel::Warn, oss.str());
			FSendBuffer::Release(sendBuffer);
			return false;
		}

		try
		{
			std::uint32_t queuedCommandCount = 0;
			{
				std::scoped_lock<std::mutex> sendCommandLock(m_workers[ownerWorkerIndex]->sendCommandMutex);
				SSendCommand command{};
				command.sessionId = sessionId;
				command.sendBuffer = sendBuffer;
				command.payloadLength = payloadLength;
				m_workers[ownerWorkerIndex]->sendCommands.push_back(command);
				queuedCommandCount =
					static_cast<std::uint32_t>(m_workers[ownerWorkerIndex]->sendCommands.size());
			}

			std::uint32_t observedMax =
				m_workers[ownerWorkerIndex]->maxObservedSendCommandCount.load(std::memory_order_relaxed);
			while (queuedCommandCount > observedMax &&
				!m_workers[ownerWorkerIndex]->maxObservedSendCommandCount.compare_exchange_weak(
					observedMax,
					queuedCommandCount,
					std::memory_order_relaxed))
			{
			}

			if (queuedCommandCount >= 8192 && (queuedCommandCount % 8192) == 0)
			{
				std::ostringstream oss;
				oss << "RIO owner-thread send queue is growing. workerIndex=" << ownerWorkerIndex
					<< " queuedSendCommands=" << queuedCommandCount
					<< " activeSessions=" << m_workers[ownerWorkerIndex]->activeSessionCount.load(std::memory_order_relaxed);
				Log(Foundation::ELogLevel::Warn, oss.str());
			}
		}
		catch (const std::exception& exception)
		{
			std::ostringstream oss;
			oss << "RIO owner-thread send enqueue failed. workerIndex=" << ownerWorkerIndex
				<< " sessionId=" << sessionId
				<< " message=" << exception.what()
				<< " maxQueuedSendCommands=" << m_workers[ownerWorkerIndex]->maxObservedSendCommandCount.load(std::memory_order_relaxed);
			Log(Foundation::ELogLevel::Error, oss.str());
			FSendBuffer::Release(sendBuffer);
			return false;
		}
		catch (...)
		{
			std::ostringstream oss;
			oss << "RIO owner-thread send enqueue failed with unknown exception. workerIndex=" << ownerWorkerIndex
				<< " sessionId=" << sessionId
				<< " maxQueuedSendCommands=" << m_workers[ownerWorkerIndex]->maxObservedSendCommandCount.load(std::memory_order_relaxed);
			Log(Foundation::ELogLevel::Error, oss.str());
			FSendBuffer::Release(sendBuffer);
			return false;
		}

		if (m_workers[ownerWorkerIndex]->completionEvent != nullptr)
		{
			SetEvent(m_workers[ownerWorkerIndex]->completionEvent);
		}

		return true;
	}

	bool FRioServer::HasPendingSendCommands(const std::uint32_t workerIndex) const
	{
		if (workerIndex >= m_workers.size())
		{
			return false;
		}

		std::scoped_lock<std::mutex> sendCommandLock(m_workers[workerIndex]->sendCommandMutex);
		return !m_workers[workerIndex]->sendCommands.empty();
	}

	bool FRioServer::AttachAcceptedSocket(SOCKET clientSocket)
	{
		{
			std::string errorMessage;
			if (!ApplyAcceptedSocketSendBufferOption(m_serverConfig, clientSocket, errorMessage))
			{
				Log(Foundation::ELogLevel::Error, errorMessage);
				return false;
			}
		}

		const std::uint32_t workerIndex = ChooseLeastLoadedWorkerIndex();
		const std::size_t recvBufferCapacity =
			static_cast<std::size_t>(std::max<std::uint32_t>(m_serverConfig.recvBufferSize * 8u, 65536u));
		const std::size_t recvStagingCapacity =
			static_cast<std::size_t>(std::max<std::uint32_t>(m_serverConfig.recvBufferSize, 2048u));
		std::optional<std::uint32_t> freeSlotIndex;
		for (std::uint32_t slotIndex = 0; slotIndex < m_serverConfig.maxSessionCount; ++slotIndex)
		{
			if (m_sessionSlots[slotIndex].load(std::memory_order_acquire) == nullptr)
			{
				freeSlotIndex = slotIndex;
				break;
			}
		}

		if (!freeSlotIndex.has_value())
		{
			Log(Foundation::ELogLevel::Warn, "All session slots are in use.");
			return false;
		}

		const std::uint32_t slotIndex = *freeSlotIndex;
		const std::uint32_t generation = m_generations[slotIndex].fetch_add(1);
		const std::uint64_t sessionId = ComposeSessionId(slotIndex, generation);
		FRioSession* newSessionContext = FRioSession::Create();
		newSessionContext->Initialize(
			clientSocket,
			sessionId,
			slotIndex,
			generation,
			workerIndex,
			recvBufferCapacity,
			recvStagingCapacity);

		const RIO_BUFFERID recvBufferId =
			m_rioFunctionTable.RIORegisterBuffer(
				newSessionContext->GetRecvStagingData(),
				static_cast<DWORD>(newSessionContext->GetRecvStagingCapacity()));
		if (recvBufferId == RIO_INVALID_BUFFERID)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "RIORegisterBuffer failed for recv staging buffer. error=" << errorCode;
			Log(Foundation::ELogLevel::Error, oss.str());
			FRioSession::Destroy(newSessionContext);
			return false;
		}
		newSessionContext->SetRecvBufferId(recvBufferId);

		RIO_RQ requestQueue =
			m_rioFunctionTable.RIOCreateRequestQueue(
				clientSocket,
				kMaxOutstandingReceive,
				kMaxReceiveDataBuffers,
				kMaxOutstandingSend,
				kMaxSendDataBuffers,
				m_workers[workerIndex]->completionQueue,
				m_workers[workerIndex]->completionQueue,
				reinterpret_cast<PVOID>(newSessionContext));
		if (requestQueue == RIO_INVALID_RQ)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "RIOCreateRequestQueue failed. error=" << errorCode;
			Log(Foundation::ELogLevel::Error, oss.str());
			newSessionContext->ReleaseRioResources(m_rioFunctionTable);
			FRioSession::Destroy(newSessionContext);
			return false;
		}
		newSessionContext->SetRequestQueue(requestQueue);

		FRioSession* expected = nullptr;
		if (!m_sessionSlots[slotIndex].compare_exchange_strong(expected, newSessionContext))
		{
			Log(Foundation::ELogLevel::Warn, "Free session slot was lost before RIO session attach completed.");
			newSessionContext->ReleaseRioResources(m_rioFunctionTable);
			FRioSession::Destroy(newSessionContext);
			return false;
		}

		{
			std::ostringstream oss;
			oss << "Client connected. sessionId=" << newSessionContext->GetSessionId()
				<< " workerIndex=" << workerIndex;
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		m_workers[workerIndex]->activeSessionCount.fetch_add(1, std::memory_order_relaxed);
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

	bool FRioServer::PostRecv(FRioSession& sessionContext)
	{
		if (sessionContext.IsClosing())
		{
			return false;
		}

		if (!sessionContext.TryBeginRecv())
		{
			return false;
		}

		const ULONG recvLength = static_cast<ULONG>(
			std::min<std::size_t>(
				sessionContext.GetRecvBuffer().GetFreeSize(),
				sessionContext.GetRecvStagingCapacity()));
		if (recvLength == 0)
		{
			sessionContext.EndRecv();
			Log(Foundation::ELogLevel::Warn, "RIO receive rejected because no writable space is available.");
			return false;
		}

		auto& recvRequestContext = sessionContext.GetRecvRequestContext();
		recvRequestContext.requestKind = FRioSession::ERequestKind::Recv;
		recvRequestContext.ownerSession = &sessionContext;
		recvRequestContext.buffer.BufferId = sessionContext.GetRecvBufferId();
		recvRequestContext.buffer.Offset = 0;
		recvRequestContext.buffer.Length = recvLength;

		bool recvResult = false;
		{
			std::scoped_lock<std::mutex> requestQueueLock(sessionContext.GetRequestQueueMutex());
			if (!sessionContext.IsClosing() && sessionContext.GetRequestQueue() != RIO_INVALID_RQ)
			{
				sessionContext.AcquireRef();
				recvResult = m_rioFunctionTable.RIOReceive(
					sessionContext.GetRequestQueue(),
					&recvRequestContext.buffer,
					1,
					0,
					&recvRequestContext) == TRUE;
				if (!recvResult)
				{
					ReleaseSession(&sessionContext);
				}
			}
		}

		if (!recvResult)
		{
			const int errorCode = WSAGetLastError();
			std::ostringstream oss;
			oss << "RIOReceive failed. sessionId=" << sessionContext.GetSessionId()
				<< " error=" << errorCode;
			Log(Foundation::ELogLevel::Warn, oss.str());
			sessionContext.EndRecv();
			return false;
		}

		return true;
	}

	void FRioServer::HandleRioCompletion(const RIORESULT& completionResult)
	{
		auto* requestContext =
			reinterpret_cast<FRioSession::SRequestContext*>(
				static_cast<ULONG_PTR>(completionResult.RequestContext));
		if (requestContext == nullptr || requestContext->ownerSession == nullptr)
		{
			return;
		}

		FRioSession& sessionContext = *requestContext->ownerSession;
		switch (requestContext->requestKind)
		{
		case FRioSession::ERequestKind::Recv:
			sessionContext.EndRecv();
			HandleRecvCompletion(sessionContext, completionResult);
			ReleaseSession(&sessionContext);
			break;
		case FRioSession::ERequestKind::Send:
			HandleSendCompletion(sessionContext, completionResult);
			ReleaseSession(&sessionContext);
			break;
		default:
			break;
		}
	}

	void FRioServer::HandleRecvCompletion(
		FRioSession& sessionContext,
		const RIORESULT& completionResult)
	{
		if (completionResult.Status != ERROR_SUCCESS || completionResult.BytesTransferred == 0)
		{
			if (completionResult.Status != ERROR_SUCCESS)
			{
				std::ostringstream oss;
				oss << "RIO recv completion failed. sessionId=" << sessionContext.GetSessionId()
					<< " status=" << completionResult.Status;
				Log(Foundation::ELogLevel::Warn, oss.str());
			}
			CloseSession(sessionContext);
			return;
		}

		m_receivedByteCount.fetch_add(completionResult.BytesTransferred, std::memory_order_relaxed);
		if (!sessionContext.CopyReceivedDataFromStaging(completionResult.BytesTransferred))
		{
			Log(Foundation::ELogLevel::Warn, "RIO recv staging copy failed.");
			CloseSession(sessionContext);
			return;
		}

		if (m_packetFramer != nullptr)
		{
			while (true)
			{
				FPacketView packetView;
				if (!m_packetFramer->TryExtractPacketView(sessionContext.GetRecvBuffer(), packetView))
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
					oss << "Packet checksum mismatch. sessionId=" << sessionContext.GetSessionId()
						<< " opcode=" << packetView.opcode
						<< " expected=" << static_cast<int>(packetView.checkSum)
						<< " actual=" << static_cast<int>(actualChecksum);
					Log(Foundation::ELogLevel::Warn, oss.str());
					CloseSession(sessionContext);
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
					oss << "Content header parse failed. sessionId=" << sessionContext.GetSessionId();
					Log(Foundation::ELogLevel::Warn, oss.str());
					CloseSession(sessionContext);
					break;
				}

				m_applicationHandler->OnPacketReceived(
					*this,
					sessionContext.GetSessionId(),
					contentPacketView);
				m_receivedPacketCount.fetch_add(1, std::memory_order_relaxed);

				const std::size_t consumedPacketSize =
					sizeof(SPacketHeader) + static_cast<std::size_t>(packetView.payloadLength);
				sessionContext.GetRecvBuffer().Discard(consumedPacketSize);
			}
		}
		else
		{
			Log(Foundation::ELogLevel::Warn, "Recv path without framer is not supported by RIO ring buffer mode.");
			CloseSession(sessionContext);
		}

		if (!sessionContext.IsClosing() && !PostRecv(sessionContext))
		{
			std::ostringstream oss;
			oss << "PostRecv failed after RIO packet dispatch. sessionId=" << sessionContext.GetSessionId();
			Log(Foundation::ELogLevel::Warn, oss.str());
			CloseSession(sessionContext);
		}
	}

	void FRioServer::HandleSendCompletion(
		FRioSession& sessionContext,
		const RIORESULT& completionResult)
	{
		auto& requestContext =
			*reinterpret_cast<FRioSession::SSendRequestContext*>(
				static_cast<ULONG_PTR>(completionResult.RequestContext));
		sessionContext.OnSendCompleted();
		if (requestContext.ownsBufferRegistration &&
			requestContext.bufferId != RIO_INVALID_BUFFERID)
		{
			m_rioFunctionTable.RIODeregisterBuffer(requestContext.bufferId);
			requestContext.bufferId = RIO_INVALID_BUFFERID;
		}
		if (requestContext.sendBuffer != nullptr)
		{
			FSendBuffer::Release(requestContext.sendBuffer);
			requestContext.sendBuffer = nullptr;
		}

		if (completionResult.Status != ERROR_SUCCESS || completionResult.BytesTransferred == 0)
		{
			if (completionResult.Status != ERROR_SUCCESS)
			{
				std::ostringstream oss;
				oss << "RIO send completion failed. sessionId=" << sessionContext.GetSessionId()
					<< " status=" << completionResult.Status;
				Log(Foundation::ELogLevel::Warn, oss.str());
			}
			CloseSession(sessionContext);
		}

		delete &requestContext;
	}

	void FRioServer::CloseSession(FRioSession& sessionContext)
	{
		if (!sessionContext.TryMarkClosing())
		{
			return;
		}

		{
			std::scoped_lock<std::mutex> requestQueueLock(sessionContext.GetRequestQueueMutex());
			if (sessionContext.GetSocket() != INVALID_SOCKET)
			{
				shutdown(sessionContext.GetSocket(), SD_BOTH);
				closesocket(sessionContext.GetSocket());
				sessionContext.SetSocket(INVALID_SOCKET);
			}
			sessionContext.SetRequestQueue(RIO_INVALID_RQ);
		}

		m_sessionSlots[sessionContext.GetSlotIndex()].store(nullptr);
		m_activeSessionCount.fetch_sub(1, std::memory_order_relaxed);
		if (sessionContext.GetOwnerWorkerIndex() < m_workers.size())
		{
			m_workers[sessionContext.GetOwnerWorkerIndex()]->activeSessionCount.fetch_sub(1, std::memory_order_relaxed);
			if (m_workers[sessionContext.GetOwnerWorkerIndex()]->completionEvent != nullptr)
			{
				SetEvent(m_workers[sessionContext.GetOwnerWorkerIndex()]->completionEvent);
			}
		}
		{
			std::ostringstream oss;
			oss << "RIO session closed. sessionId=" << sessionContext.GetSessionId()
				<< " workerIndex=" << sessionContext.GetOwnerWorkerIndex();
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		m_applicationHandler->OnClientDisconnected(sessionContext.GetSessionId());
	}

	void FRioServer::ReleaseSession(FRioSession* sessionContext)
	{
		if (sessionContext == nullptr)
		{
			return;
		}

		if (sessionContext->ReleaseRef() == 0)
		{
			sessionContext->ReleaseRioResources(m_rioFunctionTable);
			FRioSession::Destroy(sessionContext);
		}
	}

	FRioSession* FRioServer::AcquireSession(std::uint64_t sessionId)
	{
		const std::uint32_t slotIndex = static_cast<std::uint32_t>(sessionId & 0xFFFFFFFFULL);
		if (slotIndex >= m_serverConfig.maxSessionCount)
		{
			return nullptr;
		}

		FRioSession* sessionContext = m_sessionSlots[slotIndex].load();
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

	std::uint32_t FRioServer::ChooseLeastLoadedWorkerIndex() const noexcept
	{
		if (m_workers.empty())
		{
			return 0;
		}

		std::uint32_t selectedWorkerIndex = 0;
		std::uint32_t selectedLoad = m_workers[0]->activeSessionCount.load(std::memory_order_relaxed);
		for (std::uint32_t workerIndex = 1; workerIndex < m_workers.size(); ++workerIndex)
		{
			const std::uint32_t currentLoad =
				m_workers[workerIndex]->activeSessionCount.load(std::memory_order_relaxed);
			if (currentLoad < selectedLoad)
			{
				selectedLoad = currentLoad;
				selectedWorkerIndex = workerIndex;
			}
		}

		return selectedWorkerIndex;
	}

	std::uint64_t FRioServer::ComposeSessionId(std::uint32_t slotIndex, std::uint32_t generation) const noexcept
	{
		return (static_cast<std::uint64_t>(generation) << 32ULL) | static_cast<std::uint64_t>(slotIndex);
	}

	std::uint8_t FRioServer::GeneratePacketRandomKey() noexcept
	{
		return static_cast<std::uint8_t>(m_packetRandomKeySeed.fetch_add(1, std::memory_order_relaxed) & 0xFF);
	}

	void FRioServer::Log(Foundation::ELogLevel logLevel, const std::string& message) const
	{
		if (m_logger != nullptr)
		{
			m_logger->Log(logLevel, "NetworkLib", message);
		}
	}
}
