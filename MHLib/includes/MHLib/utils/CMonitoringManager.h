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

// 모니터링 매니저 클래스
namespace MHLib::utils
{
	class CMonitoringManager : public Singleton<CMonitoringManager>
	{
	private:
		CMonitoringManager() = default;
		~CMonitoringManager() = default;

	public:
		void Init(std::wstring &processName, std::vector<std::wstring> &nicNames, HANDLE hProcess = INVALID_HANDLE_VALUE, int width = 700, int height = 900)
		{
			if (hProcess == INVALID_HANDLE_VALUE)
			{
				m_hProcess = GetCurrentProcess();
			}

			HWND console = GetConsoleWindow();
			RECT r;
			GetWindowRect(console, &r);
			MoveWindow(console, r.left, r.top, width, height, TRUE);

			time_t startTime = time(nullptr);
			localtime_s(&m_startTime, &startTime);

			InitSystemMonitor();
			InitPDHMonitor(processName, nicNames);
		}

		void PrintMonitor()
		{
			for (auto &printExecute : m_MonitoringPrintFunctions)
			{
				printExecute();
			}
		}

		void UpdateMonitor()
		{
			for (auto &updateExecute : m_MonitoringUpdateFunctions)
			{
				updateExecute();
			}
		}

		inline void RegisterPrinterFunction(std::function<void()> printFunc)
		{
			m_MonitoringPrintFunctions.push_back(printFunc);
		}

		inline void RegisterUpdateFunction(std::function<void()> updateFunc)
		{
			m_MonitoringUpdateFunctions.push_back(updateFunc);
		}

	private:
		void InitSystemMonitor()
		{
			SYSTEM_INFO sysInfo;
			GetSystemInfo(&sysInfo);
			m_dwNumberOfProcessors = sysInfo.dwNumberOfProcessors;

			m_ftProcessor_LastKernel.QuadPart = 0;
			m_ftProcessor_LastUser.QuadPart = 0;
			m_ftProcessor_LastIdle.QuadPart = 0;

			m_ftProcess_LastKernel.QuadPart = 0;
			m_ftProcess_LastUser.QuadPart = 0;
			m_ftProcess_LastTime.QuadPart = 0;
		}

		void InitPDHMonitor(std::wstring &processName, std::vector<std::wstring> &nicNames)
		{
			PdhOpenQuery(NULL, NULL, &m_PDHQuery);

			PdhAddCounter(m_PDHQuery, (L"\\Process(" + processName + L")\\Pool Nonpaged Bytes").c_str(), NULL, &m_ProcessNPMemoryCounter);
			PdhAddCounter(m_PDHQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_SystemNPMemoryCounter);
			PdhAddCounter(m_PDHQuery, L"\\Memory\\Available MBytes", NULL, &m_SystemAvailableMemoryCounter);

			for (int i = 0; i < nicNames.size(); i++)
			{
				PDH_HCOUNTER NetworkSendKBCounter;
				PDH_HCOUNTER NetworkRecvKBCounter;
				PdhAddCounter(m_PDHQuery, (L"\\Network Interface(" + nicNames[i] + L")\\Bytes Sent / sec").c_str(), NULL, &NetworkSendKBCounter);
				PdhAddCounter(m_PDHQuery, (L"\\Network Interface(" + nicNames[i] + L")\\Bytes Received / sec").c_str(), NULL, &NetworkRecvKBCounter);
				m_NetworkSendKBCounters.push_back(NetworkSendKBCounter);
				m_NetworkRecvKBCounters.push_back(NetworkRecvKBCounter);
				m_NetworkSendKBVals.emplace_back();
				m_NetworkRecvKBVals.emplace_back();
			}

			PdhAddCounter(m_PDHQuery, L"\\TCPv4\\Segments Retransmitted/sec", NULL, &m_NetworkRetransmissionCounter);

			UpdatePDH();
		}

		void UpdatePDH()
		{
			PdhCollectQueryData(m_PDHQuery);

			PdhGetFormattedCounterValue(m_ProcessNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_ProcessNPMemoryVal);
			PdhGetFormattedCounterValue(m_SystemNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemNPMemoryVal);
			PdhGetFormattedCounterValue(m_SystemAvailableMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemAvailableMemoryVal);

			for (int i = 0; i < m_NetworkSendKBVals.size(); i++)
			{
				PdhGetFormattedCounterValue(m_NetworkSendKBCounters[i], PDH_FMT_LARGE, NULL, &m_NetworkSendKBVals[i]);
				PdhGetFormattedCounterValue(m_NetworkRecvKBCounters[i], PDH_FMT_LARGE, NULL, &m_NetworkRecvKBVals[i]);
			}

			PdhGetFormattedCounterValue(m_NetworkRetransmissionCounter, PDH_FMT_LARGE, NULL, &m_NetworkRetransmissionVal);
		}

	private:
		// 시작 시간 기록용
		tm m_startTime;
		tm m_currentTime;

		// 시스템 모니터
		HANDLE m_hProcess = INVALID_HANDLE_VALUE; // 프로세스 핸들
		DWORD m_dwNumberOfProcessors;

		ULARGE_INTEGER m_ftProcessor_LastKernel;
		ULARGE_INTEGER m_ftProcessor_LastUser;
		ULARGE_INTEGER m_ftProcessor_LastIdle;

		ULARGE_INTEGER m_ftProcess_LastKernel;
		ULARGE_INTEGER m_ftProcess_LastUser;
		ULARGE_INTEGER m_ftProcess_LastTime;

		// PDH 모니터
		PDH_HQUERY  m_PDHQuery;

		PDH_HCOUNTER m_ProcessNPMemoryCounter;
		PDH_HCOUNTER m_SystemNPMemoryCounter;
		PDH_HCOUNTER m_SystemAvailableMemoryCounter;
		std::vector<PDH_HCOUNTER> m_NetworkSendKBCounters;
		std::vector<PDH_HCOUNTER> m_NetworkRecvKBCounters;
		PDH_HCOUNTER m_NetworkRetransmissionCounter;

		PDH_FMT_COUNTERVALUE m_ProcessNPMemoryVal;
		PDH_FMT_COUNTERVALUE m_SystemNPMemoryVal;
		PDH_FMT_COUNTERVALUE m_SystemAvailableMemoryVal;
		std::vector<PDH_FMT_COUNTERVALUE> m_NetworkSendKBVals;
		std::vector<PDH_FMT_COUNTERVALUE> m_NetworkRecvKBVals;
		PDH_FMT_COUNTERVALUE m_NetworkRetransmissionVal;

		// 외부에서 모니터링할 대상의 void() 함수 직접 정의해서 등록
		std::vector<std::function<void()>> m_MonitoringPrintFunctions; // 모니터링 출력
		std::vector<std::function<void()>> m_MonitoringUpdateFunctions; // 모니터링 업데이트
	};

}