#include "pch.h"
#include "CWorldServer.h"
#include "InitWorldServer.h"
#include "LoadConfig.h"

int main()
{
	WorldServer::Utils::InitWorldServer();
	WorldServer::Utils::LoadConfig();

	NetworkLib::Core::Net::Server::g_NetServer = new WorldServer::Server::CWorldServer;
	if (NetworkLib::Core::Net::Server::g_NetServer->Start(
		NetworkLib::Core::Net::Server::Config::openIP.c_str(),
		NetworkLib::Core::Net::Server::Config::openPort) == FALSE)
	{
		return 1;
	}

	Sleep(INFINITE);
	return 0;
}
