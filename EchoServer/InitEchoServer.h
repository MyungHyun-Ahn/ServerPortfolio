#pragma once

#include "MonitoringClientLib/RegisterMonitor.h"

namespace EchoServer::Utils
{
	void InitEchoServer()
	{
		MHLib::utils::g_Logger->SetMainDirectory(L"LogFile");
		MHLib::utils::g_Logger->SetLogLevel(MHLib::utils::LOG_LEVEL::DEBUG);
		MHLib::utils::g_MonitoringMgr->SetConsoleSize(700, 960);
		MonitoringClientLib::Monitoring::RegisterMonitor(std::wstring(L"EchoServer"), { std::wstring(L"Realtek PCIe GBE Family Controller"), std::wstring(L"Realtek PCIe GBE Family Controller _2") });
		EchoServer::Monitor::g_EchoMonitor = EchoServer::Monitor::CEchoMonitor::GetInstance();
		MHLib::utils::g_MonitoringMgr->RegisterMonitor(EchoServer::Monitor::g_EchoMonitor);
	}
}