#pragma once

namespace EchoServer::Objects
{
	class CNonLoginPlayer
	{
	public:
		DWORD m_dwPrevRecvTime;

	private:
		USE_TLS_POOL(CNonLoginPlayer, s_NonLoginPlayerPool)
	};

	class CPlayer
	{
	public:
		INT64 m_iAccountNo;
		DWORD m_dwPrecRecvTime;

	private:
		USE_TLS_POOL(CPlayer, s_PlayerPool)
	};
}