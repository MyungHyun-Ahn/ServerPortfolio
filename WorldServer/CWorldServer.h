#pragma once

#include "NetworkLib/CNetServer.h"

namespace WorldServer::Contents
{
	class CEntranceContents;
}

namespace WorldServer::Server
{
	class CWorldServer : public NetworkLib::Core::Net::Server::CNetServer
	{
	public:
		friend class WorldServer::Contents::CEntranceContents;

		bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
		void OnAccept(const UINT64 sessionID) noexcept override;
		void OnClientLeave(const UINT64 sessionID) noexcept override;
		void OnError(int errorcode, WCHAR *errMsg) noexcept override;
		void RegisterContentTimerEvent() noexcept override;

	private:
		WorldServer::Contents::CEntranceContents *m_pEntranceContents = nullptr;
	};
}
