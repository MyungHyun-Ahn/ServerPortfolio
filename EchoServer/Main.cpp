#include "pch.h"
#include "NetworkLib/CLanClient.h"
#include "MonitoringClientLib/CMonitoringClient.h"
#include "MonitoringClientLib/MonitoringSetting.h"

#include "CEchoServer.h"

int main()
{
	// TODO : Config Load, Monitor Setting

	NetworkLib::Core::Lan::Client::g_netClientMgr = new NetworkLib::Core::Lan::Client::CLanClientManager;
	NetworkLib::Core::Lan::Client::g_netClientMgr->Init();

	MonitoringClientLib::g_MonitoringClient = new MonitoringClientLib::CMonitoringClient;
	MonitoringClientLib::g_MonitoringClient->Init(1); // 1°³ ¿¬°á
	
	NetworkLib::Core::Lan::Client::g_netClientMgr->RegisterClient(MonitoringClientLib::g_MonitoringClient);
	NetworkLib::Core::Lan::Client::g_netClientMgr->Start();

	MonitoringClientLib::g_MonitoringClient->PostConnectEx(MonitoringClientLib::Setting::IP.c_str(), MonitoringClientLib::Setting::PORT);

	NetworkLib::Core::Net::Server::g_NetServer = new EchoServer::Server::CEchoServer;
	NetworkLib::Core::Net::Server::g_NetServer->Start(NetworkLib::Core::Net::Server::openIP.c_str(), NetworkLib::Core::Net::Server::openPort);

	NetworkLib::Core::Lan::Client::g_netClientMgr->Wait();

	return 0;
}