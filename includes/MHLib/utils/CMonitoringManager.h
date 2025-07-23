#pragma once

#include "DefineSingleton.h"

#pragma comment(lib, "Pdh.lib")

#include <Windows.h>
#include <string>
#include <Psapi.h>
#include <Pdh.h>
#include <ctime>
#include <list>
#include <functional>

#include "CLogger.h"
#include "CMonitor.h"

// 모니터링 매니저 클래스
namespace MHLib::utils
{
	class CMonitoringManager : public Singleton<CMonitoringManager>
	{
	private:
		CMonitoringManager() = default;
		~CMonitoringManager() = default;

	public:
		void Update()
		{
			for (CMonitor *monitor : m_Monitors)
			{
				monitor->MonitoringFunc();
			}
		}

	private:
		std::vector<CMonitor *> m_Monitors;
	};

}