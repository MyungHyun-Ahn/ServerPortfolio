#pragma once

#include "ILogger.h"

namespace GameServer::Foundation
{
	class FConsoleLogger final : public ILogger
	{
	public:
		explicit FConsoleLogger(const SLogConfig& logConfig);
		void Log(ELogLevel logLevel, std::string_view category, std::string_view message) override;

	private:
		SLogConfig m_logConfig;
	};
}
