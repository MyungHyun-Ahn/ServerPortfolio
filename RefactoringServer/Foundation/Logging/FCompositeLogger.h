#pragma once

#include "ILogger.h"

#include <memory>
#include <vector>

namespace GameServer::Foundation
{
	class FCompositeLogger final : public ILogger
	{
	public:
		void AddSink(std::shared_ptr<ILogger> logger);
		void Log(ELogLevel logLevel, std::string_view category, std::string_view message) override;

	private:
		std::vector<std::shared_ptr<ILogger>> m_sinks;
	};
}
