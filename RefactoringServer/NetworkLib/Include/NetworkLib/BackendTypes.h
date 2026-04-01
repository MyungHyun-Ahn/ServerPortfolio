#pragma once

#include <cstdint>
#include <string>

namespace GameServer::NetworkLib
{
	enum class EBackendKind : std::uint32_t
	{
		Iocp = 0,
		Rio = 1,
		BoostAsio = 2
	};

	struct SServerConfig
	{
		EBackendKind backendKind = EBackendKind::Iocp;
		std::string bindIp = "127.0.0.1";
		std::uint16_t port = 19000;
		std::uint32_t workerThreadCount = 2;
		std::uint32_t maxSessionCount = 64;
		std::uint32_t recvBufferSize = 1024;
	};
}
