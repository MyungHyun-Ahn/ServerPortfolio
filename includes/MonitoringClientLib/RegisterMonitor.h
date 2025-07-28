#pragma once
namespace MonitoringClientLib::Monitoring
{
	void RegisterMonitor(const std::wstring &processName, const std::vector<std::wstring> &nicNames, HANDLE hProcess = INVALID_HANDLE_VALUE);
}

