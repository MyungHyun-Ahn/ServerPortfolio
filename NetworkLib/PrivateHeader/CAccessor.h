#pragma once

namespace NetworkLib::Core::Utils
{
	class CAccessor
	{
		inline static NetworkLib::Core::Monitoring::ServerMonitoringTargets &GetServerMonitoringTargets()
		{
			return NetworkLib::Core::Net::Server::g_NetServer->m_serverMonitoringTargets;
		}
	};
}

