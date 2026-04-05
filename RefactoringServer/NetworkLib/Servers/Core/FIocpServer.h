#pragma once

namespace NetworkLib
{
	class IApplicationHandler;
}

namespace NetworkLib::Session
{
	class FIocpSession;
}

namespace NetworkLib::Core
{
	using NetworkLib::IApplicationHandler;

	class FIocpServer final : public IServer
	{
	public:
		FIocpServer();
		~FIocpServer() override;

		bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) override;
		void Stop() override;
		bool SendPacket(
			std::uint64_t sessionId,
			NetworkLib::Packet::Serialization::FOutgoingContentPacket&& packet) override;
		bool Disconnect(std::uint64_t sessionId) override;
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
		bool PostRecv(NetworkLib::Session::FIocpSession& sessionContext);
		bool PostSend(NetworkLib::Session::FIocpSession& sessionContext);
		void CloseSession(NetworkLib::Session::FIocpSession& sessionContext);
		void ReleaseSession(NetworkLib::Session::FIocpSession* sessionContext);
		NetworkLib::Session::FIocpSession* AcquireSession(std::uint64_t sessionId);
		bool AttachAcceptedSocket(SOCKET clientSocket);
		std::uint64_t ComposeSessionId(std::uint32_t slotIndex, std::uint32_t generation) const;
		std::uint8_t GeneratePacketRandomKey() noexcept;
		void Log(Foundation::ELogLevel logLevel, const std::string& message) const;

	private:
		inline static constexpr std::size_t kMaxSendBatchCount = 32;
		SServerConfig m_serverConfig{};
		IApplicationHandler* m_applicationHandler = nullptr;
		std::shared_ptr<Foundation::ILogger> m_logger;
		std::shared_ptr<NetworkLib::Crypto::IPacketCipher> m_packetCipher;
		std::shared_ptr<NetworkLib::Packet::Framing::IPacketFramer> m_packetFramer;
		HANDLE m_iocpHandle = nullptr;
		SOCKET m_listenSocket = INVALID_SOCKET;
		std::thread m_acceptThread;
		std::vector<std::thread> m_workerThreads;
		std::unique_ptr<std::atomic<NetworkLib::Session::FIocpSession*>[]> m_sessionSlots;
		std::unique_ptr<std::atomic<std::uint32_t>[]> m_generations;
		std::atomic<std::uint32_t> m_packetRandomKeySeed = 1;
		std::atomic<std::uint32_t> m_activeSessionCount = 0;
		std::atomic<std::uint64_t> m_acceptedSessionCount = 0;
		std::atomic<std::uint64_t> m_receivedPacketCount = 0;
		std::atomic<std::uint64_t> m_sentPacketCount = 0;
		std::atomic<std::uint64_t> m_receivedByteCount = 0;
		std::atomic<std::uint64_t> m_sentByteCount = 0;
		std::atomic<std::uint64_t> m_wsaRecvCallCount = 0;
		std::atomic<std::uint64_t> m_wsaSendCallCount = 0;
		std::atomic<bool> m_isRunning = false;
		std::atomic<bool> m_winsockInitialized = false;
	};
}
