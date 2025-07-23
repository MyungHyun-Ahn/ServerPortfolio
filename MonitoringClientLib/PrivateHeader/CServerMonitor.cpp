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
	void CServerMonitor::Update()
	{
		m_llLoopCount++;
	}

	void CServerMonitor::PrintConsole()
	{
		NetworkLib::Core::Monitoring::ServerMonitoringTargets *serverMonitoringTargets 
			= NetworkLib::Core::Utils::CAccessor::GetServerMonitoringTargets();
		NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets *contentsThreadMonitoringTarget 
			= NetworkLib::Core::Utils::CAccessor::GetContentsThreadMonitoringTargets();
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\t\t\tServer monitoring info");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Info");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tSession Count \t: %lld", serverMonitoringTargets->sessionCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Pool");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM
			, L"\tSession pool capacity \t: %d, usage \t:%d", NetworkLib::Core::Net::Server::CNetSession::GetPoolCapacity(), NetworkLib::Core::Net::Server::CNetSession::GetPoolUsage());
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM
			, L"\tSBuffer pool capacity \t: %d, usage \t:%d", NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::GetPoolCapacity(), NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::GetPoolUsage());
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Network");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tAccept total \t: %lld", m_llAcceptTotal);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tSend total \t: %lld", m_llSendTotal);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tRecv total \t: %lld", m_llRecvTotal);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tAccept TPS \t: %d, Avg \t: %lld", serverMonitoringTargets->acceptTPS, m_llAcceptTotal / m_llLoopCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tSend TPS \t: %d, Avg \t: %lld", serverMonitoringTargets->sendTPS, m_llSendTotal / m_llLoopCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tRecv TPS \t: %d, Avg \t: %lld", serverMonitoringTargets->recvTPS, m_llRecvTotal / m_llLoopCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Debug Info");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tMaxSend\t : %d", serverMonitoringTargets->maxSendCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tSendPending\t : %d", serverMonitoringTargets->sendPendingCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tSendCall\t : %d", serverMonitoringTargets->sendCallTPS);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"ContentsThread");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tDelWork\t\t : %d", contentsThreadMonitoringTarget->delegateWorkCount);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tStealWork\t : %d", contentsThreadMonitoringTarget->workStealingCount);
	}

	void CServerMonitor::Send()
	{
		// 세션 ID 1부터 할당됨
		if (g_currentMonitoringClientSessionId == 0)
			return;

		int currentTime = time(NULL);

		NetworkLib::Core::Monitoring::ServerMonitoringTargets *serverMonitoringTargets 
			= NetworkLib::Core::Utils::CAccessor::GetServerMonitoringTargets();
		NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets *contentsThreadMonitoringTarget 
			= NetworkLib::Core::Utils::CAccessor::GetContentsThreadMonitoringTargets();

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGameServerSession
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_SESSION
				, serverMonitoringTargets->sessionCount, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameServerSession);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGameAcceptTPS
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_ACCEPT_TPS
				, serverMonitoringTargets->acceptTPS, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGameAcceptTPS);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGamePacketSendTPS
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_PACKET_SEND_TPS
				, serverMonitoringTargets->sendTPS, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGamePacketSendTPS);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGamePacketRecvTPS
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_PACKET_RECV_TPS
				, serverMonitoringTargets->recvTPS, currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGamePacketRecvTPS);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGamePacketPacketPool
			= Protocol::CGenPacket::makePacketReqMonitoringUpdate(Protocol::MONITOR_DATA_UPDATE::GAME_PACKET_POOL
				, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::GetPoolCapacity(), currentTime);
		g_MonitoringClient->SendPacket(g_currentMonitoringClientSessionId, pGamePacketPacketPool);
	}

	void CServerMonitor::Reset()
	{
		NetworkLib::Core::Monitoring::ServerMonitoringTargets *serverMonitoringTargets 
			= NetworkLib::Core::Utils::CAccessor::GetServerMonitoringTargets();
		NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets *contentsThreadMonitoringTarget 
			= NetworkLib::Core::Utils::CAccessor::GetContentsThreadMonitoringTargets();

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