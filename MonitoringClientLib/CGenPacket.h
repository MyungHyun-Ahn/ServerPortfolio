#pragma once

namespace MonitoringClientLib::Protocol
{
	using namespace NetworkLib;
	using namespace NetworkLib::DataStructures;

	class CGenPacket
	{
	public:
		static CSerializableBuffer<SERVER_TYPE::LAN> *makePacketReqMonitoringLogin(const INT serverNo) noexcept;
		static CSerializableBuffer<SERVER_TYPE::LAN> *makePacketReqMonitoringUpdate(const MONITOR_DATA_UPDATE dataType, const INT dataValue, const INT timeStamp) noexcept;
	};
}
