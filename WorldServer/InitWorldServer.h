#pragma once

namespace WorldServer::Utils
{
	void InitWorldServer()
	{
		MHLib::utils::g_Logger->SetMainDirectory(L"LogFile");
		MHLib::utils::g_Logger->SetLogLevel(MHLib::utils::LOG_LEVEL::DEBUG);
	}
}
