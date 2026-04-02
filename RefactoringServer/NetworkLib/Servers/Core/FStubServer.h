#pragma once

namespace NetworkLib::Core
{
	class FStubServer final : public IServer
	{
	public:
		explicit FStubServer(EBackendKind backendKind);

		bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) override;
		void Stop() override;
		bool Send(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) override;
		EBackendKind GetBackendKind() const override;
		SServerStats GetStatsSnapshot() const override;

	private:
		EBackendKind m_backendKind;
	};
}
