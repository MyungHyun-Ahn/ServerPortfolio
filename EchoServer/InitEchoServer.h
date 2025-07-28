#pragma once
namespace EchoServer::Utils
{
	void InitEchoServer()
	{
		MHLib::utils::g_Logger->SetMainDirectory(L"LogFile");
		MHLib::utils::g_Logger->SetLogLevel(MHLib::utils::LOG_LEVEL::DEBUG);
		MHLib::utils::g_MonitoringMgr->SetConsoleSize();
	}
}