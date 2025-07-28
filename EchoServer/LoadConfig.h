#pragma once

namespace EchoServer::Utils
{
	void LoadConfig()
	{
		MHLib::utils::CFileLoader configLoader;
		configLoader.Parse(L"ServerConfig.conf");

		NetworkLib::Core::Net::Server::Config::Load(configLoader);
		NetworkLib::Core::Lan::Client::Config::Load(configLoader);
		MonitoringClientLib::Config::Load(configLoader);
		EchoServer::Server::Config::Load(configLoader);
	}
}