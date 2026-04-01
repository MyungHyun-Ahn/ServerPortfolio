#pragma once

#include "Servers/BackendTypes.h"

#include <string>
#include <string_view>

namespace GameServer::NetworkLib::Logging
{
	std::string BuildDateStamp();
	std::string BuildLine(const SLogConfig& logConfig, ELogLevel logLevel, std::string_view category, std::string_view message);
	bool ShouldWrite(const SLogConfig& logConfig, ELogLevel logLevel);
}
