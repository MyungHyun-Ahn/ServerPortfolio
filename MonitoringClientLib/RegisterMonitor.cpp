#include "pch.h"
#include "RegisterMonitor.h"
#include "PrivateHeader/CSystemMonitor.h"
#include "PrivateHeader/CServerMonitor.h"

namespace MonitoringClientLib::Monitoring
{
	void RegisterMonitor(const std::wstring &processName, const std::vector<std::wstring> &nicNames, HANDLE hProcess)
	{
		g_SystemMonitor = CSystemMonitor::GetInstance();
		g_ServerMonitor = CServerMonitor::GetInstance();
		g_SystemMonitor->Init(processName, nicNames, hProcess);

		MHLib::utils::g_MonitoringMgr->RegisterMonitor(g_SystemMonitor);
		MHLib::utils::g_MonitoringMgr->RegisterMonitor(g_ServerMonitor);
	}
}