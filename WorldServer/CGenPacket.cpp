#include "pch.h"
#include "CGenPacket.h"
#include "WorldProtocol.h"

namespace WorldServer::Protocol
{
	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *CGenPacket::MakePacketResLogin(BYTE status, INT64 accountNo)
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pBuffer
			= NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Alloc();
		*pBuffer << (WORD)PACKET_TYPE::CS_GAME_RES_LOGIN << status << accountNo;
		return pBuffer;
	}

	NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *CGenPacket::MakePacketResEcho(INT64 accountNo, LONGLONG sendTick)
	{
		NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pBuffer
			= NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Alloc();
		*pBuffer << (WORD)PACKET_TYPE::CS_GAME_RES_ECHO << accountNo << sendTick;
		return pBuffer;
	}
}
