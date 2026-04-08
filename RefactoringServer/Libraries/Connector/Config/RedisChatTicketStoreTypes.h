#pragma once

#include <cstdint>
#include <string>

namespace Connector
{
	enum class EChatTicketStoreMode
	{
		Disabled,
		Redis
	};

	struct SRedisConnectionConfig
	{
		std::string host = "127.0.0.1";
		std::uint16_t port = 6379;
		std::string password;
		std::int32_t database = 0;
		std::uint32_t connectTimeoutMs = 3000;
	};

	struct SRedisChatTicketStoreConfig
	{
		SRedisConnectionConfig connection;
		std::string keyPrefix = "chat:ticket:";
	};
}
