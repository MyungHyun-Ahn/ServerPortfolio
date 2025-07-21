#pragma once

namespace NetworkLib::Core::Utils
{
	using namespace NetworkLib::Contents;
	using namespace NetworkLib::Core::Monitoring;
	using namespace NetworkLib::Core::Net::Server;

	class CAccessor
	{
		inline static ServerMonitoringTargets &GetServerMonitoringTargets()
		{
			return g_NetServer->m_monitoringTargets;
		}

		inline static ContentsThreadMonitoringTargets &GetContentsThreadMonitoringTargets()
		{
			return CContentsThread::s_monitoringTargets;
		}
	};
}

