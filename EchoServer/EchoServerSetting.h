#pragma once

namespace EchoServer::Server
{
	namespace Config
	{
		extern INT AUTH_FPS;
		extern INT ECHO_FPS;

		void Load(MHLib::utils::CFileLoader &loader);
	}
}
