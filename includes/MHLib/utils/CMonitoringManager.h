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
		friend class Singleton<CMonitoringManager>;

	private:
		CMonitoringManager() = default;
		~CMonitoringManager() = default;

	public:
		void SetConsoleSize(int width = 700, int height = 900)
		{
			HWND console = GetConsoleWindow();
			RECT r;
			GetWindowRect(console, &r);
			MoveWindow(console, r.left, r.top, 700, 900, TRUE);
		}

		inline void RegisterMonitor(CMonitor *monitor)
		{
			m_Monitors.push_back(monitor);
		}

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

	inline CMonitoringManager *g_MonitoringMgr = nullptr;
}