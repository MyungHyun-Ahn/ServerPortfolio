#include "pch.h"
#include "MonitoringProtocol.h"
#include "CGenPacket.h"

namespace MonitoringClientLib::Protocol
{
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *CGenPacket::makePacketReqMonitoringLogin(const INT serverNo) noexcept
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *sBuffer = NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Alloc();
		*sBuffer << (WORD)PACKET_TYPE::SS_MONITOR_LOGIN << serverNo;
		return sBuffer;
	}
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *CGenPacket::makePacketReqMonitoringUpdate(const MONITOR_DATA_UPDATE dataType, const INT dataValue, const INT timeStamp) noexcept
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *sBuffer = NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Alloc();
		*sBuffer << (WORD)PACKET_TYPE::SS_MONITOR_DATA_UPDATE << (BYTE)dataType << dataValue << timeStamp;
		return sBuffer;
	}
}