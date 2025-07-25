#pragma once
namespace EchoServer::Protocol
{
	class CGenPacket
	{
	public:
		static NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *makePacketResLogin(BYTE status, INT64 accountNo);
		static NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *makePacketResEcho(INT64 accountNo, LONGLONG sendTick);
	};
}

