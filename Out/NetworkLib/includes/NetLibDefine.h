#pragma once

namespace NetworkLib
{
	enum class SERVER_TYPE
	{
		LAN,
		NET
	};

	enum class HEADER_SIZE : int
	{
		LAN = 2,
		NET = 5
	};
}