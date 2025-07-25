#include "pch.h"
#include "CGenPacket.h"
#include "EchoProtocol.h"

namespace EchoServer::Protocol
{
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *CGenPacket::makePacketResLogin(BYTE status, INT64 accountNo)
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *sBuffer 
			= NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Alloc();
		*sBuffer << (WORD)PACKET_TYPE::CS_GAME_RES_LOGIN << status << accountNo;
		return sBuffer;
	}
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *CGenPacket::makePacketResEcho(INT64 accountNo, LONGLONG sendTick)
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *sBuffer 
			= NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Alloc();
		*sBuffer << (WORD)PACKET_TYPE::CS_GAME_RES_ECHO << accountNo << sendTick;
		return sBuffer;
	}
}