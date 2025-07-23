#include "pch.h"
#include "CServerMonitor.h"
#include "CNetServer.h"
#include "PrivateHeader/MonitoringTarget.h"
#include "PrivateHeader/CAccessor.h"

#include "../MonitoringProtocol.h"
#include "../CGenPacket.h"

#include "CLanClient.h"
#include "../CMonitoringClient.h"

namespace MonitoringClientLib::Monitoring
{
	using NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets;
	using NetworkLib::Core::Monitoring::ServerMonitoringTargets;
	using NetworkLib::Core::Utils::CAccessor;
	using namespace NetworkLib::Core::Net::Server;
	using namespace NetworkLib::DataStructures;
	using namespace NetworkLib;

	void CServerMonitor::Update()
	{
		m_llLoopCount++;
	}

	void CServerMonitor::PrintConsole()
	{
		ServerMonitoringTargets *serverMonitoringTargets = CAccessor::GetServerMonitoringTargets();
		ContentsThreadMonitoringTargets *contentsThreadMonitoringTarget = CAccessor::GetContentsThreadMonitoringTargets();
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\t\t\tServer monitoring info");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Info");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession Count \t: %lld", serverMonitoringTargets->sessionCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Pool");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession pool capacity \t: %d, usage \t:%d", CNetSession::GetPoolCapacity(), CNetSession::GetPoolUsage());
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSBuffer pool capacity \t: %d, usage \t:%d", CSerializableBuffer<SERVER_TYPE::NET>::GetPoolCapacity(), CSerializableBuffer<SERVER_TYPE::NET>::GetPoolUsage());
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Network");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept total \t: %lld", m_llAcceptTotal);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend total \t: %lld", m_llSendTotal);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv total \t: %lld", m_llRecvTotal);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept TPS \t: %d, Avg \t: %lld", serverMonitoringTargets->acceptTPS, m_llAcceptTotal / m_llLoopCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend TPS \t: %d, Avg \t: %lld", serverMonitoringTargets->sendTPS, m_llSendTotal / m_llLoopCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv TPS \t: %d, Avg \t: %lld", serverMonitoringTargets->recvTPS, m_llRecvTotal / m_llLoopCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Debug Info");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tMaxSend\t : %d", serverMonitoringTargets->maxSendCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSendPending\t : %d", serverMonitoringTargets->sendPendingCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSendCall\t : %d", serverMonitoringTargets->sendCallTPS);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"ContentsThread");
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tDelWork\t\t : %d", contentsThreadMonitoringTarget->delegateWorkCount);
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tStealWork\t : %d", contentsThreadMonitoringTarget->workStealingCount);
	}

	void CServerMonitor::Send()
	{
		// 세션 ID 1부터 할당됨
		if (g_currentMonitoringClientSessionId == 0)
			return;

		int currentTime = time(NULL);

		ServerMonitoringTargets *serverMonitoringTargets = CAccessor::GetServerMonitoringTargets();
		ContentsThreadMonitoringTargets *contentsThreadMonitoringTarget = CAccessor::GetContentsThreadMonitoringTargets();

		CSerializableBuffer<SERVER_TYPE::LAN> *pGameServerSession
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_SESSION
				, serverMonitoringTargets->sessionCount, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameServerSession);

		CSerializableBuffer<SERVER_TYPE::LAN> *pGameAcceptTPS
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_ACCEPT_TPS
				, serverMonitoringTargets->acceptTPS, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameAcceptTPS);

		CSerializableBuffer<SERVER_TYPE::LAN> *pGamePacketSendTPS
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_PACKET_SEND_TPS
				, serverMonitoringTargets->sendTPS, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGamePacketSendTPS);

		CSerializableBuffer<SERVER_TYPE::LAN> *pGamePacketRecvTPS
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_PACKET_RECV_TPS
				, serverMonitoringTargets->recvTPS, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGamePacketRecvTPS);

		CSerializableBuffer<SERVER_TYPE::LAN> *pGamePacketPacketPool
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_PACKET_POOL
				, CSerializableBuffer<SERVER_TYPE::NET>::GetPoolCapacity(), currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGamePacketPacketPool);
	}

	void CServerMonitor::Reset()
	{
		ServerMonitoringTargets *serverMonitoringTargets = CAccessor::GetServerMonitoringTargets();
		ContentsThreadMonitoringTargets *contentsThreadMonitoringTarget = CAccessor::GetContentsThreadMonitoringTargets();

		m_llAcceptTotal	+= InterlockedExchange(&serverMonitoringTargets->acceptTPS, 0);
		m_llSendTotal	+= InterlockedExchange(&serverMonitoringTargets->sendTPS, 0);
		m_llRecvTotal	+= InterlockedExchange(&serverMonitoringTargets->recvTPS, 0);

		InterlockedExchange(&serverMonitoringTargets->maxSendCount, 0);
		InterlockedExchange(&serverMonitoringTargets->sendPendingCount, 0);
		InterlockedExchange(&serverMonitoringTargets->sendCallTPS, 0);
		InterlockedExchange(&contentsThreadMonitoringTarget->delegateWorkCount, 0);
		InterlockedExchange(&contentsThreadMonitoringTarget->workStealingCount, 0);
	}
}