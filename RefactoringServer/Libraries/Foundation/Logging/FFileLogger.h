#pragma once

#include "ILogger.h"

namespace Foundation
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
