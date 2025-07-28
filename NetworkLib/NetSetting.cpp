#include "pch.h"
#include "NetSetting.h"

namespace NetworkLib::Core::Net
{
	namespace Server::Config
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

		void Load(MHLib::utils::CFileLoader &loader)
		{
			loader.Load(L"Server", L"IP", &openIP);
			loader.Load(L"Server", L"PORT", &openPort);
			loader.Load(L"Server", L"PACKET_KEY", &MHLib::scurity::CEncryption::PACKET_KEY);
			loader.Load(L"Server", L"PACKET_CODE", &PACKET_CODE);
			loader.Load(L"Server", L"IOCP_WORKER_THREAD", &IOCP_WORKER_THREAD);
			loader.Load(L"Server", L"IOCP_ACTIVE_THREAD", &IOCP_ACTIVE_THREAD);
			loader.Load(L"Server", L"USE_ZERO_COPY", &USE_ZERO_COPY);
			loader.Load(L"Server", L"MAX_SESSION_COUNT", &MAX_SESSION_COUNT);
			loader.Load(L"Server", L"ACCEPTEX_COUNT", &ACCEPTEX_COUNT);
			loader.Load(L"Server", L"CONTENTS_THREAD_COUNT", &CONTENTS_THREAD_COUNT);
			loader.Load(L"Server", L"MAX_CONTENTS_FPS", &MAX_CONTENTS_FPS);
			loader.Load(L"Server", L"DELAY_FRAME", &DELAY_FRAME);
		}
	}
}
