#pragma once

#include "NetworkLib/NetSetting.h"
#include "WorldServerSetting.h"

namespace WorldServer::Utils
{
	void LoadConfig()
	{
		MHLib::utils::CFileLoader configLoader;
		// skeleton 단계에서는 기존에 검증된 UTF-16 설정을 재사용해 바인딩 실패 가능성을 줄인다.
		configLoader.Parse(L"..\\EchoServer\\ServerConfig.conf");

		NetworkLib::Core::Net::Server::Config::Load(configLoader);
	}
}
