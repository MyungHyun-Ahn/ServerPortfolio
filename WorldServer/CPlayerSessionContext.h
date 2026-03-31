#pragma once

namespace WorldServer::Objects
{
	class CPlayerSessionContext
	{
	public:
		INT64 m_iAccountNo = 0;
		DWORD m_dwPrevRecvTime = 0;
		bool m_isLoggedIn = false;

	private:
		USE_TLS_POOL(CPlayerSessionContext, s_PlayerSessionContextPool)
	};
}
