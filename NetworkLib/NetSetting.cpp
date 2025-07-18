#include "pch.h"
#include "NetSetting.h"

namespace NetworkLib::Core::Net
{
	namespace Server
	{
		BYTE PACKET_KEY = 0xa9;
		BYTE PACKET_CODE = 0x77;

		std::string openIP;
		USHORT openPort;

		INT IOCP_WORKER_THREAD = 16;
		INT IOCP_ACTIVE_THREAD = 4;
		INT USE_ZERO_COPY = 0;
		INT MAX_SESSION_COUNT = 20000;
		INT ACCEPTEX_COUNT = 4;

		INT CONTENTS_THREAD_COUNT = 4;
		INT MAX_CONTENTS_FPS = 25;
		INT DELAY_FRAME = 10;
	}
}