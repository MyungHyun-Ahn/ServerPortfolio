#include "pch.h"
#include "WorldServerSetting.h"

namespace WorldServer::Server
{
	namespace Config
	{
		INT ENTRANCE_FPS = 25;

		void Load(MHLib::utils::CFileLoader &loader)
		{
			loader.Load(L"WorldServer", L"ENTRANCE_FPS", &ENTRANCE_FPS);
		}
	}
}
