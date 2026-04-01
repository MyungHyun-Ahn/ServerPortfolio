#pragma once

#include "Servers/BackendTypes.h"

#include <cstdint>

namespace GameServer::NetworkLib
{
	class IApplicationHandler;

	class IServer
	{
	public:
		virtual ~IServer() = default;

		virtual bool Start(const SServerConfig& serverConfig, IApplicationHandler& applicationHandler) = 0;
		virtual void Stop() = 0;
		virtual bool Send(std::uint64_t sessionId, const char* buffer, std::int32_t length) = 0;
		virtual EBackendKind GetBackendKind() const = 0;
	};
}
