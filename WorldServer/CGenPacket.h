#pragma once

namespace WorldServer::Protocol
{
	class CGenPacket
	{
	public:
		static NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *MakePacketResLogin(BYTE status, INT64 accountNo);
		static NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *MakePacketResEcho(INT64 accountNo, LONGLONG sendTick);
	};
}
