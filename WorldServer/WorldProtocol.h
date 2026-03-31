#pragma once

namespace WorldServer::Protocol
{
	enum class PACKET_TYPE
	{
		CS_GAME_SERVER = 1000,
		CS_GAME_REQ_LOGIN,
		CS_GAME_RES_LOGIN,

		CS_GAME_REQ_ECHO = 5000,
		CS_GAME_RES_ECHO,

		CS_GAME_REQ_HEARTBEAT,
	};
}
