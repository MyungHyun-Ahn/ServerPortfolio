#include "pch.h"
#include "NetworkLib/CLanClient.h"
#include "MonitoringClientLib/CMonitoringClient.h"
#include "MonitoringClientLib/MonitoringSetting.h"

int main()
{
	NetworkLib::Core::Lan::Client::g_netClientMgr = new NetworkLib::Core::Lan::Client::CLanClientManager;
	NetworkLib::Core::Lan::Client::g_netClientMgr->Init();

	MonitoringClientLib::g_MonitoringClient = new MonitoringClientLib::CMonitoringClient;
	MonitoringClientLib::g_MonitoringClient->Init(1); // 1개 연결
	
	NetworkLib::Core::Lan::Client::g_netClientMgr->RegisterClient(MonitoringClientLib::g_MonitoringClient);
	NetworkLib::Core::Lan::Client::g_netClientMgr->Start();

	MonitoringClientLib::g_MonitoringClient->PostConnectEx(MonitoringClientLib::Setting::IP.c_str(), MonitoringClientLib::Setting::PORT);

	// 여기부터 서버

	NetworkLib::Core::Lan::Client::g_netClientMgr->Wait();

	return 0;
}