#include "Pch.h"

#include "FConsoleLogger.h"

#include "LogFormatting.h"

namespace
{
	std::mutex g_consoleMutex;
}

namespace GameServer::Foundation
{
	FConsoleLogger::FConsoleLogger(const SLogConfig& logConfig)
		: m_logConfig(logConfig)
	{
	}

	void FConsoleLogger::Log(ELogLevel logLevel, std::string_view category, std::string_view message)
	{
		if (!m_logConfig.consoleEnabled || !Logging::ShouldWrite(m_logConfig, logLevel))
		{
			return;
		}

		const std::lock_guard<std::mutex> lock(g_consoleMutex);
		std::cout << Logging::BuildLine(m_logConfig, logLevel, category, message) << std::endl;
	}
}
