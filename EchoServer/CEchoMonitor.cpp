#include "pch.h"
#include "CEchoMonitor.h"
#include "CPlayer.h"
#include "CAuthContents.h"
#include "CEchoContents.h"
#include "CEchoServer.h"

#include "MonitoringClientLib/MonitoringProtocol.h"
#include "MonitoringClientLib/CGenPacket.h"

#include "NetworkLib/CLanClient.h"
#include "MonitoringClientLib/CMonitoringClient.h"


namespace EchoServer::Monitor
{
	void CEchoMonitor::Update()
	{
		EchoServer::Server::CEchoServer *pEchoServer = reinterpret_cast<EchoServer::Server::CEchoServer *>(NetworkLib::Core::Net::Server::g_NetServer);
		m_lAuthFPS = pEchoServer->m_pAuthContents->FPSReset();
		m_lEchoFPS = pEchoServer->m_pEchoContents->FPSReset();
	}

	void CEchoMonitor::PrintConsole()
	{
		EchoServer::Server::CEchoServer *pEchoServer = reinterpret_cast<EchoServer::Server::CEchoServer *>(NetworkLib::Core::Net::Server::g_NetServer);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\t\t\tEcho monitoring info");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Info");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tNonLoginPlayer Count \t: %lld", pEchoServer->m_pAuthContents->GetSessionCount());
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tLoginPlayer Count \t: %lld", pEchoServer->m_pEchoContents->GetSessionCount());
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"Frame");
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tAuth Contents FPS \t: %d", m_lAuthFPS);
		MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"\tEcho Contents FPS \t: %d", m_lEchoFPS);
	}

	void CEchoMonitor::Send()
	{
		int currentTime = time(NULL);
		EchoServer::Server::CEchoServer *pEchoServer = reinterpret_cast<EchoServer::Server::CEchoServer *>(NetworkLib::Core::Net::Server::g_NetServer);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pGameServerRun
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_SERVER_RUN
				, TRUE, currentTime);
		MonitoringClientLib::g_MonitoringClient->SendPacket(MonitoringClientLib::g_currentMonitoringClientSessionId, pGameServerRun);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pAuthPlayer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_AUTH_PLAYER
				, pEchoServer->m_pAuthContents->GetSessionCount(), currentTime);
		MonitoringClientLib::g_MonitoringClient->SendPacket(MonitoringClientLib::g_currentMonitoringClientSessionId, pAuthPlayer);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pEchoPlayer
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_GAME_PLAYER
				, pEchoServer->m_pEchoContents->GetSessionCount(), currentTime);
		MonitoringClientLib::g_MonitoringClient->SendPacket(MonitoringClientLib::g_currentMonitoringClientSessionId, pEchoPlayer);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pAuthFPS
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_AUTH_THREAD_FPS
				, m_lAuthFPS, currentTime);
		MonitoringClientLib::g_MonitoringClient->SendPacket(MonitoringClientLib::g_currentMonitoringClientSessionId, pAuthFPS);

		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pEchoFPS
			= MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringUpdate(MonitoringClientLib::Protocol::MONITOR_DATA_UPDATE::GAME_GAME_THREAD_FPS
				, m_lEchoFPS, currentTime);
		MonitoringClientLib::g_MonitoringClient->SendPacket(MonitoringClientLib::g_currentMonitoringClientSessionId, pEchoFPS);
	}

	void CEchoMonitor::Reset()
	{

	}
}