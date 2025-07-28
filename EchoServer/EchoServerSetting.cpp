#include "pch.h"
#include "EchoServerSetting.h"

namespace EchoServer::Server
{
	namespace Config
	{
		INT AUTH_FPS = 25;
		INT ECHO_FPS = 25;

		void Load(MHLib::utils::CFileLoader &loader)
		{
			loader.Load(L"GameServer", L"AUTH_FPS", &AUTH_FPS);
			loader.Load(L"GameServer", L"ECHO_FPS", &ECHO_FPS);
		}
	}
}