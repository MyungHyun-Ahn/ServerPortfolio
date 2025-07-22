#pragma once

namespace MonitoringClientLib
{
	using namespace NetworkLib;
	using namespace NetworkLib::DataStructures;

	class CMonitoringClient : public NetworkLib::Core::Lan::Client::CLanClient
	{
		// CLanClient��(��) ���� ��ӵ�
		void OnConnect(const UINT64 sessionID) noexcept override;
		void OnDisconnect(const UINT64 sessionID) noexcept override;
		void OnRecv(const UINT64 sessionID, CSerializableBuffer<SERVER_TYPE::LAN> *message) noexcept override;
	};

	extern UINT64 g_currentMonitoringClientSessionId;
	extern CMonitoringClient *g_MonitoringClient;
}