#include "pch.h"
#include "CGenPacket.h"

namespace EchoServer::Protocol
{
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *CGenPacket::makePacketResLogin(BYTE status, INT64 accountNo)
	{
		return nullptr;
	}
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *CGenPacket::makePacketResEcho(INT64 accountNo, LONGLONG sendTick)
	{
		return nullptr;
	}
}