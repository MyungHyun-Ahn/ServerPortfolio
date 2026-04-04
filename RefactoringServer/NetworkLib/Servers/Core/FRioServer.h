#pragma once

namespace NetworkLib::Core
{
	class FRioServer final : public IServer
	{
	public:
		FRioServer() = default;

		bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) override;
		void Stop() override;
		bool Send(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) override;
		bool Disconnect(std::uint64_t sessionId) override;
		EBackendKind GetBackendKind() const override;
		SServerStats GetStatsSnapshot() const override;

	private:
		std::shared_ptr<Foundation::ILogger> m_logger;
	};
}

