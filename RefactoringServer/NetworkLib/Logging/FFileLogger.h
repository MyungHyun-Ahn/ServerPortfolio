#pragma once

#include "Servers/BackendTypes.h"
#include "Logging/ILogger.h"

namespace GameServer::NetworkLib
{
	class FFileLogger final : public ILogger
	{
	public:
		explicit FFileLogger(const SLogConfig& logConfig);
		void Log(ELogLevel logLevel, std::string_view category, std::string_view message) override;

	private:
		SLogConfig m_logConfig;
	};
}
