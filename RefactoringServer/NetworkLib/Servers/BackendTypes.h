#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace GameServer::NetworkLib
{
	class ILogger;

	enum class EBackendKind : std::uint32_t
	{
		Iocp = 0,
		Rio = 1,
		BoostAsio = 2
	};

	enum class ELogLevel : std::uint32_t
	{
		Debug = 0,
		Info = 1,
		Warn = 2,
		Error = 3
	};

	struct SLogConfig
	{
		ELogLevel minimumLevel = ELogLevel::Info;
		std::string outputDirectory = "logs";
		bool consoleEnabled = true;
		bool fileEnabled = true;
		bool includeThreadId = true;
	};

	struct SServerConfig
	{
		EBackendKind backendKind = EBackendKind::Iocp;
		std::string bindIp = "127.0.0.1";
		std::uint16_t port = 19000;
		std::uint32_t workerThreadCount = 2;
		std::uint32_t maxSessionCount = 64;
		std::uint32_t recvBufferSize = 1024;
		SLogConfig logConfig{};
		std::shared_ptr<ILogger> logger;
	};
}
