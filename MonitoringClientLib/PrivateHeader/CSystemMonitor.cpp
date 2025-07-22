#include "pch.h"
#include "../CMonitor.h"
#include "CSystemMonitor.h"

#include "CLanClient.h"
#include "../CMonitoringClient.h"

#include "../MonitoringProtocol.h"
#include "../CGenPacket.h"

namespace MonitoringClientLib::Monitoring
{
	CSystemMonitor *g_SystemMonitor;

	void CSystemMonitor::Init(HANDLE hProcess)
	{
		if (hProcess == INVALID_HANDLE_VALUE)
		{
			m_hProcess = GetCurrentProcess();
		}

		time_t startTime = time(nullptr);
		localtime_s(&m_startTime, &startTime);
	}

	void CSystemMonitor::Update()
	{
		UpdateSystemMonitor();
		UpdateMemoryMonitor();
		UpdatePDHMonitor();
	}

	void CSystemMonitor::PrintConsole()
	{

	}

	void CSystemMonitor::Send()
	{

	}

	// 시스템 모니터에서는 딱히 할일이 없음
	void CSystemMonitor::Reset()
	{
	}

	void CSystemMonitor::InitSystemMonitor()
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

	void CSystemMonitor::UpdateSystemMonitor()
	{
		// 프로세서 사용률을 갱신
	// FILETIME 구조체를 사용하지만 ULARGE_INTEGER와 구조가 같으므로 이를 사용
		ULARGE_INTEGER idle;
		ULARGE_INTEGER kernel;
		ULARGE_INTEGER user;

		// 시스템 사용 시간을 구함
		// 아이들 타임 / 커널 사용 타임(아이들 포함) / 유저 사용 타임
		if (GetSystemTimes((PFILETIME)&idle, (PFILETIME)&kernel, (PFILETIME)&user) == false)
			return;

		ULONGLONG kernelDiff = kernel.QuadPart - m_ftProcessor_LastKernel.QuadPart;
		ULONGLONG userDiff = user.QuadPart - m_ftProcessor_LastUser.QuadPart;
		ULONGLONG idleDiff = idle.QuadPart - m_ftProcessor_LastIdle.QuadPart;

		ULONGLONG total = kernelDiff + userDiff;
		ULONGLONG timeDiff;

		if (total == 0)
		{
			m_fProcessorUser = 0.0f;
			m_fProcessorKernel = 0.0f;
			m_fProcessorTotal = 0.0f;
		}
		else
		{
			m_fProcessorTotal = (float)((double)(total - idleDiff) / total * 100.0f);
			m_fProcessorUser = (float)((double)(userDiff) / total * 100.0f);
			m_fProcessorKernel = (float)((double)(kernelDiff - idleDiff) / total * 100.0f);
		}

		m_ftProcessor_LastKernel = kernel;
		m_ftProcessor_LastUser = user;
		m_ftProcessor_LastIdle = idle;

		// 지정된 프로세스 사용률을 갱신
		ULARGE_INTEGER none;
		ULARGE_INTEGER nowTime;

		// 100 나노세컨드 단위 시간을 구함. UTC
		// 프로세스 사용률 판단 공식
		// a = 샘플 간격의 시스템 시간을 구함 (실제 흐른 시간)
		// b = 프로세스의 CPU 사용 시간을 구함
		// a : 100 = b : 사용률 공식을 통해 사용률을 계산


		// 100 나노세컨드 단위의 시간 구함
		GetSystemTimeAsFileTime((LPFILETIME)&nowTime);

		// 해당 프로세스가 사용한 시간을 구함
		// 두번째, 세번째는 실행, 종료 시간으로 미사용
		GetProcessTimes(m_hProcess, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernel, (LPFILETIME)&user);

		// 이전에 저장된 프로세스 시간과의 차를 구해 실제로 얼마의 시간이 지났는지 확인
		// 실제 지나온 시간으로 나누면 사용률
		timeDiff = nowTime.QuadPart - m_ftProcess_LastTime.QuadPart;
		userDiff = user.QuadPart - m_ftProcess_LastUser.QuadPart;
		kernelDiff = kernel.QuadPart - m_ftProcess_LastKernel.QuadPart;

		total = kernelDiff + userDiff;

		m_fProcessTotal = (float)(total / (double)m_dwNumberOfProcessors / (double)timeDiff * 100.0f);
		m_fProcessKernel = (float)(kernelDiff / (double)m_dwNumberOfProcessors / (double)timeDiff * 100.0f);
		m_fProcessUser = (float)(userDiff / (double)m_dwNumberOfProcessors / (double)timeDiff * 100.0f);

		m_ftProcess_LastTime = nowTime;
		m_ftProcess_LastKernel = kernel;
		m_ftProcess_LastUser = user;
	}


	void CSystemMonitor::InitPDHMonitor(std::wstring &processName, std::vector<std::wstring> &nicNames)
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

		UpdatePDHMonitor();
	}

	void CSystemMonitor::UpdatePDHMonitor()
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

	void CSystemMonitor::UpdateMemoryMonitor()
	{
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m_stPmc, sizeof(m_stPmc));
	}

	void CSystemMonitor::PrintSystemMonitorInfo()
	{
		// 시간 계산
		time_t currentTime = time(nullptr);
		localtime_s(&m_currentTime, &currentTime);

		// Cpu Usage
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
		// 년 월 일 시 분 초
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tstart \t: %04d.%02d.%02d.%02d.%02d.%02d",
			m_startTime.tm_year + 1900, m_startTime.tm_mon + 1, m_startTime.tm_mday,
			m_startTime.tm_hour, m_startTime.tm_min, m_startTime.tm_sec);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnow \t: %04d.%02d.%02d.%02d.%02d.%02d",
			m_currentTime.tm_year + 1900, m_currentTime.tm_mon + 1, m_currentTime.tm_mday,
			m_currentTime.tm_hour, m_currentTime.tm_min, m_currentTime.tm_sec);

		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\t\t\tSystem info");

		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process memory usage");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\ttotal \t: %u MB", m_stPmc.PrivateUsage / (1024 * 1024));
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u KB\n", m_ProcessNPMemoryVal.largeValue / (1024));
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"System memory usage");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tavailable \t: %u MB", m_SystemAvailableMemoryVal.largeValue);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u MB\n", m_SystemNPMemoryVal.largeValue / (1024 * 1024));
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Processor CPU usage");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process CPU usage");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessKernel);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Network");
		for (int i = 1; i <= m_NetworkSendKBVals.size(); i++)
		{
			g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend%d \t: %u KB", i, m_NetworkSendKBVals[i - 1].largeValue / (1024));
		}
		for (int i = 1; i <= m_NetworkSendKBVals.size(); i++)
		{
			g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv1 \t: %u KB", i, m_NetworkRecvKBVals[i - 1].largeValue / (1024));
		}

		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRetransmission \t: %u\n", m_NetworkRetransmissionVal.largeValue);
	}

	void CSystemMonitor::SendSystemMonitorInfo()
	{
		// 세션 ID 1부터 할당됨
		if (g_currentMonitoringClientSessionId == 0)
			return;

		int currentTime = time(NULL);

		CSerializableBuffer<SERVER_TYPE::LAN> *pCpuTotalBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::MONITOR_CPU_TOTAL, m_fProcessTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pCpuTotalBuffer);

		CSerializableBuffer<SERVER_TYPE::LAN> *pNonpagedMemBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::MONITOR_NONPAGED_MEMORY
				, m_SystemNPMemoryVal.largeValue / (1024 * 1024), currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pNonpagedMemBuffer);

		INT sendKBTotal = 0;
		for (int i = 1; i <= m_NetworkSendKBVals.size(); i++)
		{
			sendKBTotal += m_NetworkSendKBVals[i - 1].largeValue / (1024);
		}

		CSerializableBuffer<SERVER_TYPE::LAN> *pNetworkSendBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::MONITOR_NETWORK_SEND
				, sendKBTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pNetworkSendBuffer);

		INT recvKBTotal = 0;
		for (int i = 1; i <= m_NetworkRecvKBVals.size(); i++)
		{
			recvKBTotal += m_NetworkRecvKBVals[i - 1].largeValue / (1024);
		}

		CSerializableBuffer<SERVER_TYPE::LAN> *pNetworkRecvBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::MONITOR_NETWORK_RECV
				, recvKBTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pNetworkRecvBuffer);

		CSerializableBuffer<SERVER_TYPE::LAN> *pAvailableMemBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::MONITOR_AVAILABLE_MEMORY
				, m_SystemAvailableMemoryVal.largeValue, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pAvailableMemBuffer);

		// 아랫 부분 학원 제공 프로토콜이 GAME, CHAT, ... 이런식으로 서버가 특정되어 있음
		// 나중에 직접 모니터링 프로토콜 설계할 때는 따로 다시 만들어야 함

		CSerializableBuffer<SERVER_TYPE::LAN> *pGameServerCpuBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_SERVER_CPU
				, m_fProcessTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameServerCpuBuffer);

		CSerializableBuffer<SERVER_TYPE::LAN> *pGameServerMemBuffer
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_SERVER_MEM
				, m_stPmc.PrivateUsage / (1024 * 1024), currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameServerMemBuffer);
	}
}