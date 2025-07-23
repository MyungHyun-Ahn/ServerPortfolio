#pragma once

namespace MonitoringClientLib::Protocol
{
	class CGenPacket
	{
	public:
		static NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *makePacketReqMonitoringLogin(const INT serverNo) noexcept;
		static NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *makePacketReqMonitoringUpdate(const MONITOR_DATA_UPDATE dataType, const INT dataValue, const INT timeStamp) noexcept;
	};
}
