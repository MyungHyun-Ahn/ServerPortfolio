#pragma once

#include <cstdint>

namespace GameServer::NetworkLib::Packet
{
#pragma pack(push, 1)
	struct SContentHeader
	{
		std::uint16_t opcode = 0;
	};
#pragma pack(pop)
}
