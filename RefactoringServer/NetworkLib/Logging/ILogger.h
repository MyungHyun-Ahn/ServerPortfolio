#pragma once

#include "Servers/BackendTypes.h"

#include <string_view>

namespace GameServer::NetworkLib
{
	class ILogger
	{
	public:
		virtual ~ILogger() = default;

		virtual void Log(ELogLevel logLevel, std::string_view category, std::string_view message) = 0;
	};
}
