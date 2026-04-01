#pragma once

#include "Servers/BackendTypes.h"
#include "Servers/IApplicationHandler.h"
#include "Logging/ILogger.h"
#include "Servers/IServer.h"

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace GameServer::NetworkLib
{
	class FIocpServer final : public IServer
	{
	public:
		FIocpServer();
		~FIocpServer() override;

		bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) override;
		void Stop() override;
		bool Send(std::uint64_t sessionId, const char* buffer, std::int32_t length) override;
		EBackendKind GetBackendKind() const override;

	private:
		enum class EIoType : std::uint32_t
		{
			Recv,
			Send
		};

		struct SSessionContext;

		struct SIoContext
		{
			OVERLAPPED overlapped{};
			WSABUF wsabuf{};
			EIoType ioType = EIoType::Recv;
			SSessionContext* ownerSession = nullptr;
			std::vector<char> buffer;
		};

		struct SSessionContext
		{
			SOCKET socket = INVALID_SOCKET;
			std::uint64_t sessionId = 0;
			std::uint32_t slotIndex = 0;
			std::uint32_t generation = 0;
			std::atomic<long> refCount = 1;
			std::atomic<bool> closing = false;
			SIoContext recvContext{};
		};

		bool InitializeWinsock();
		bool OpenListenSocket();
		void CloseListenSocket();
		void StartWorkers();
		void StopWorkers();
		void AcceptLoop();
		void WorkerLoop();
		bool PostRecv(SSessionContext& sessionContext);
		void CloseSession(SSessionContext& sessionContext);
		void ReleaseSession(SSessionContext* sessionContext);
		SSessionContext* AcquireSession(std::uint64_t sessionId);
		bool AttachAcceptedSocket(SOCKET clientSocket);
		std::uint64_t ComposeSessionId(std::uint32_t slotIndex, std::uint32_t generation) const;
		void Log(ELogLevel logLevel, const std::string& message) const;

	private:
		SServerConfig m_serverConfig{};
		IApplicationHandler* m_applicationHandler = nullptr;
		std::shared_ptr<ILogger> m_logger;
		HANDLE m_iocpHandle = nullptr;
		SOCKET m_listenSocket = INVALID_SOCKET;
		std::thread m_acceptThread;
		std::vector<std::thread> m_workerThreads;
		std::unique_ptr<std::atomic<SSessionContext*>[]> m_sessionSlots;
		std::unique_ptr<std::atomic<std::uint32_t>[]> m_generations;
		std::atomic<bool> m_isRunning = false;
		std::atomic<bool> m_winsockInitialized = false;
	};
}
