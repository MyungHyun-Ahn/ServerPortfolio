#include "Pch.h"

#include "Logging/FCompositeLogger.h"

namespace GameServer::NetworkLib
{
	void FCompositeLogger::AddSink(std::shared_ptr<ILogger> logger)
	{
		if (logger != nullptr)
		{
			m_sinks.push_back(std::move(logger));
		}
	}

	void FCompositeLogger::Log(ELogLevel logLevel, std::string_view category, std::string_view message)
	{
		for (const std::shared_ptr<ILogger>& sink : m_sinks)
		{
			sink->Log(logLevel, category, message);
		}
	}
}
