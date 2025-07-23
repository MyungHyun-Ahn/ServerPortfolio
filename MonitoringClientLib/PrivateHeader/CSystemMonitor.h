#pragma once

namespace MonitoringClientLib::Monitoring
{
	class CSystemMonitor : public MHLib::utils::CMonitor, public MHLib::utils::Singleton<CSystemMonitor>
	{
	private:
		CSystemMonitor() = default;
		virtual ~CSystemMonitor() = default;

	public:
		void Init(HANDLE hProcess = INVALID_HANDLE_VALUE);

	private:
		virtual void Update() override;
		virtual void PrintConsole() override;
		virtual void Send() override;
		virtual void Reset() override;

		void InitSystemMonitor();
		void UpdateSystemMonitor();
		void InitPDHMonitor(std::wstring &processName, std::vector<std::wstring> &nicNames);
		void UpdatePDHMonitor();
		void UpdateMemoryMonitor();

		void PrintSystemMonitorInfo();
		void SendSystemMonitorInfo();

	private:
		// 시작 시간 기록용
		tm m_startTime;
		tm m_currentTime;

		// 시스템 모니터
		HANDLE m_hProcess = INVALID_HANDLE_VALUE; // 프로세스 핸들
		DWORD m_dwNumberOfProcessors;

		FLOAT m_fProcessorTotal = 0;
		FLOAT m_fProcessorUser = 0;
		FLOAT m_fProcessorKernel = 0;

		ULARGE_INTEGER m_ftProcessor_LastKernel;
		ULARGE_INTEGER m_ftProcessor_LastUser;
		ULARGE_INTEGER m_ftProcessor_LastIdle;

		FLOAT m_fProcessTotal = 0;
		FLOAT m_fProcessKernel = 0;
		FLOAT m_fProcessUser = 0;

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

		PROCESS_MEMORY_COUNTERS_EX m_stPmc;
	};

	extern CSystemMonitor *g_SystemMonitor;
}

