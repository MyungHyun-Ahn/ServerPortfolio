#pragma once

namespace WorldServer::Server
{
	namespace Config
	{
		extern INT ENTRANCE_FPS;

		void Load(MHLib::utils::CFileLoader &loader);
	}
}
