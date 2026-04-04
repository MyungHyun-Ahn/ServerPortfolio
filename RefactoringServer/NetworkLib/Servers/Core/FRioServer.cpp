#include "Pch.h"

#include "Servers/Core/BackendTypes.h"
#include "Servers/Core/FRioServer.h"
#include "Foundation/Logging/ILogger.h"

namespace NetworkLib::Core
{
	bool FRioServer::Start(const SServerConfig& serverConfig, IApplicationHandler&)
	{
		m_logger = serverConfig.logger;
		if (m_logger != nullptr)
		{
			m_logger->Log(Foundation::ELogLevel::Warn, "NetworkLib", "RIO backend is not implemented yet.");
		}
		else
		{
			std::cerr << "RIO backend is not implemented yet.\n";
		}

		return false;
	}

	void FRioServer::Stop()
	{
		m_logger.reset();
	}

	bool FRioServer::Send(std::uint64_t, std::uint16_t, const char*, std::int32_t)
	{
		return false;
	}

	bool FRioServer::Disconnect(std::uint64_t)
	{
		return false;
	}

	EBackendKind FRioServer::GetBackendKind() const
	{
		return EBackendKind::Rio;
	}

	SServerStats FRioServer::GetStatsSnapshot() const
	{
		return {};
	}
}
