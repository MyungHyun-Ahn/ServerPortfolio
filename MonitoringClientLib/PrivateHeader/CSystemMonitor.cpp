#include "pch.h"
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
		PrintSystemMonitorInfo();
	}

	void CSystemMonitor::Send()
	{
		SendSystemMonitorInfo();
	}

	// �ý��� ����Ϳ����� ���� ������ ����
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
		// ���μ��� ������ ����
	// FILETIME ����ü�� ��������� ULARGE_INTEGER�� ������ �����Ƿ� �̸� ���
		ULARGE_INTEGER idle;
		ULARGE_INTEGER kernel;
		ULARGE_INTEGER user;

		// �ý��� ��� �ð��� ����
		// ���̵� Ÿ�� / Ŀ�� ��� Ÿ��(���̵� ����) / ���� ��� Ÿ��
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

		// ������ ���μ��� ������ ����
		ULARGE_INTEGER none;
		ULARGE_INTEGER nowTime;

		// 100 ���뼼���� ���� �ð��� ����. UTC
		// ���μ��� ���� �Ǵ� ����
		// a = ���� ������ �ý��� �ð��� ���� (���� �帥 �ð�)
		// b = ���μ����� CPU ��� �ð��� ����
		// a : 100 = b : ���� ������ ���� ������ ���


		// 100 ���뼼���� ������ �ð� ����
		GetSystemTimeAsFileTime((LPFILETIME)&nowTime);

		// �ش� ���μ����� ����� �ð��� ����
		// �ι�°, ����°�� ����, ���� �ð����� �̻��
		GetProcessTimes(m_hProcess, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernel, (LPFILETIME)&user);

		// ������ ����� ���μ��� �ð����� ���� ���� ������ ���� �ð��� �������� Ȯ��
		// ���� ������ �ð����� ������ ����
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
		// �ð� ���
		time_t currentTime = time(nullptr);
		localtime_s(&m_currentTime, &currentTime);

		// Cpu Usage
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"----------------------------------------");
		// �� �� �� �� �� ��
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tstart \t: %04d.%02d.%02d.%02d.%02d.%02d",
			m_startTime.tm_year + 1900, m_startTime.tm_mon + 1, m_startTime.tm_mday,
			m_startTime.tm_hour, m_startTime.tm_min, m_startTime.tm_sec);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tnow \t: %04d.%02d.%02d.%02d.%02d.%02d",
			m_currentTime.tm_year + 1900, m_currentTime.tm_mon + 1, m_currentTime.tm_mday,
			m_currentTime.tm_hour, m_currentTime.tm_min, m_currentTime.tm_sec);

		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\t\t\tSystem info");

		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Process memory usage");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\ttotal \t: %u MB", m_stPmc.PrivateUsage / (1024 * 1024));
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u KB\n", m_ProcessNPMemoryVal.largeValue / (1024));
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"System memory usage");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tavailable \t: %u MB", m_SystemAvailableMemoryVal.largeValue);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u MB\n", m_SystemNPMemoryVal.largeValue / (1024 * 1024));
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Processor CPU usage");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Process CPU usage");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessKernel);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Network");
		for (int i = 1; i <= m_NetworkSendKBVals.size(); i++)
		{
			MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tSend%d \t: %u KB", i, m_NetworkSendKBVals[i - 1].largeValue / (1024));
		}
		for (int i = 1; i <= m_NetworkSendKBVals.size(); i++)
		{
			MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tRecv1 \t: %u KB", i, m_NetworkRecvKBVals[i - 1].largeValue / (1024));
		}

		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tRetransmission \t: %u\n", m_NetworkRetransmissionVal.largeValue);
	}

	void CSystemMonitor::SendSystemMonitorInfo()
	{
		// ���� ID 1���� �Ҵ��
		if (g_currentMonitoringClientSessionId == 0)
			return;

		int currentTime = time(NULL);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pCpuTotalBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::MONITOR_CPU_TOTAL, m_fProcessTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pCpuTotalBuffer);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pNonpagedMemBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::MONITOR_NONPAGED_MEMORY
				, m_SystemNPMemoryVal.largeValue / (1024 * 1024), currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pNonpagedMemBuffer);

		INT sendKBTotal = 0;
		for (int i = 1; i <= m_NetworkSendKBVals.size(); i++)
		{
			sendKBTotal += m_NetworkSendKBVals[i - 1].largeValue / (1024);
		}

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pNetworkSendBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::MONITOR_NETWORK_SEND
				, sendKBTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pNetworkSendBuffer);

		INT recvKBTotal = 0;
		for (int i = 1; i <= m_NetworkRecvKBVals.size(); i++)
		{
			recvKBTotal += m_NetworkRecvKBVals[i - 1].largeValue / (1024);
		}

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pNetworkRecvBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::MONITOR_NETWORK_RECV
				, recvKBTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pNetworkRecvBuffer);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pAvailableMemBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::MONITOR_AVAILABLE_MEMORY
				, m_SystemAvailableMemoryVal.largeValue, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pAvailableMemBuffer);

		// �Ʒ� �κ� �п� ���� ���������� GAME, CHAT, ... �̷������� ������ Ư���Ǿ� ����
		// ���߿� ���� ����͸� �������� ������ ���� ���� �ٽ� ������ ��

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGameServerCpuBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_SERVER_CPU
				, m_fProcessTotal, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameServerCpuBuffer);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGameServerMemBuffer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_SERVER_MEM
				, m_stPmc.PrivateUsage / (1024 * 1024), currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameServerMemBuffer);
	}
}