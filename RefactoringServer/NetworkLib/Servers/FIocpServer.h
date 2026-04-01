#pragma once

#include "Servers/BackendTypes.h"
#include "Servers/FSession.h"
#include "Servers/IServer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace GameServer::NetworkLib
{
	class IApplicationHandler;

	class FIocpServer final : public IServer
	{
	public:
		FIocpServer();
		~FIocpServer() override;

		bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) override;
		void Stop() override;
		bool Send(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) override;
		EBackendKind GetBackendKind() const override;
		SServerStats GetStatsSnapshot() const override;

	private:
		bool InitializeWinsock();
		bool OpenListenSocket();
		void CloseListenSocket();
		void StartWorkers();
		void StopWorkers();
		void AcceptLoop();
		void WorkerLoop();
		bool PostRecv(FSession& sessionContext);
		bool PostSend(FSession& sessionContext);
		void CloseSession(FSession& sessionContext);
		void ReleaseSession(FSession* sessionContext);
		FSession* AcquireSession(std::uint64_t sessionId);
		bool AttachAcceptedSocket(SOCKET clientSocket);
		std::uint64_t ComposeSessionId(std::uint32_t slotIndex, std::uint32_t generation) const;
		std::uint8_t GeneratePacketRandomKey() noexcept;
		void Log(GameServer::Foundation::ELogLevel logLevel, const std::string& message) const;

	private:
		inline static constexpr std::size_t kMaxSendBatchCount = 32;
		SServerConfig m_serverConfig{};
		IApplicationHandler* m_applicationHandler = nullptr;
		std::shared_ptr<GameServer::Foundation::ILogger> m_logger;
		std::shared_ptr<GameServer::NetworkLib::Crypto::IPacketCipher> m_packetCipher;
		std::shared_ptr<GameServer::NetworkLib::Packet::IPacketFramer> m_packetFramer;
		HANDLE m_iocpHandle = nullptr;
		SOCKET m_listenSocket = INVALID_SOCKET;
		std::thread m_acceptThread;
		std::vector<std::thread> m_workerThreads;
		std::unique_ptr<std::atomic<FSession*>[]> m_sessionSlots;
		std::unique_ptr<std::atomic<std::uint32_t>[]> m_generations;
		std::atomic<std::uint32_t> m_packetRandomKeySeed = 1;
		std::atomic<std::uint32_t> m_activeSessionCount = 0;
		std::atomic<std::uint64_t> m_acceptedSessionCount = 0;
		std::atomic<std::uint64_t> m_receivedPacketCount = 0;
		std::atomic<std::uint64_t> m_sentPacketCount = 0;
		std::atomic<std::uint64_t> m_wsaRecvCallCount = 0;
		std::atomic<std::uint64_t> m_wsaSendCallCount = 0;
		std::atomic<bool> m_isRunning = false;
		std::atomic<bool> m_winsockInitialized = false;
	};
}
