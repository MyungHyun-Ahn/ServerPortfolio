#pragma once

namespace NetworkLib::Core::Utils
{
	class CAccessor
	{
	public:
		inline static NetworkLib::Core::Monitoring::ServerMonitoringTargets *GetServerMonitoringTargets()
		{
			return NetworkLib::Core::Net::Server::g_NetServer->m_monitoringTargets;
		}

		inline static NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets *GetContentsThreadMonitoringTargets()
		{
			return NetworkLib::Contents::CContentsThread::s_monitoringTargets;
		}
	};
}

