#pragma once

#include "Servers/IApplicationHandler.h"
#include "Servers/IServer.h"

namespace GameServer::NetworkLib
{
	class FStubServer final : public IServer
	{
	public:
		explicit FStubServer(EBackendKind backendKind);

		bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) override;
		void Stop() override;
		bool Send(std::uint64_t sessionId, const char* buffer, std::int32_t length) override;
		EBackendKind GetBackendKind() const override;

	private:
		EBackendKind m_backendKind;
	};
}
