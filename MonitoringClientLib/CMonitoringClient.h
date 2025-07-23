#pragma once

namespace MonitoringClientLib
{
	class CMonitoringClient : public NetworkLib::Core::Lan::Client::CLanClient
	{
		// CLanClient��(��) ���� ��ӵ�
		void OnConnect(const UINT64 sessionID) noexcept override;
		void OnDisconnect(const UINT64 sessionID) noexcept override;
		void OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *message) noexcept override;
	};

	extern UINT64 g_currentMonitoringClientSessionId;
	extern CMonitoringClient *g_MonitoringClient;
}