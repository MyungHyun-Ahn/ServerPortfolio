#include "pch.h"
#include "CLanClient.h"
#include "CMonitoringClient.h"
#include "MonitoringProtocol.h"
#include "CGenPacket.h"
#include "MonitoringSetting.h"

namespace MonitoringClientLib
{
	UINT64 g_currentMonitoringClientSessionId = 0;
	CMonitoringClient *g_MonitoringClient;

	void CMonitoringClient::OnConnect(const UINT64 sessionID) noexcept
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *reqLogin = MonitoringClientLib::Protocol::CGenPacket::makePacketReqMonitoringLogin(Setting::SERVER_NO);
		reqLogin->IncreaseRef();

		SendPacket(sessionID, reqLogin);
		g_currentMonitoringClientSessionId = sessionID;

		if (reqLogin->DecreaseRef() == 0)
			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Free(reqLogin);
	}
	void CMonitoringClient::OnDisconnect(const UINT64 sessionID) noexcept
	{
	}

	// 모니터링 클라는 딱히 받을게 없음
	void CMonitoringClient::OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *message) noexcept
	{
	}
}