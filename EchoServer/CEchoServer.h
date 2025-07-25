#pragma once
namespace EchoServer::Contents
{
	class CAuthContents;
	class CEchoContents;
}

namespace EchoServer::Monitor
{
	class CEchoMonitor;
}

namespace EchoServer::Server
{
	class CEchoServer : public NetworkLib::Core::Net::Server::CNetServer
	{
	public:
		friend class EchoServer::Contents::CAuthContents;
		friend class EchoServer::Contents::CEchoContents;
		friend class EchoServer::Monitor::CEchoMonitor;

		// CNetServer을(를) 통해 상속됨
		bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept override;
		void OnAccept(const UINT64 sessionID) noexcept override;
		void OnClientLeave(const UINT64 sessionID) noexcept override;
		void OnError(int errorcode, WCHAR *errMsg) noexcept override;
		void RegisterContentTimerEvent() noexcept override;

	private:
		EchoServer::Contents::CAuthContents *m_pAuthContents;
		EchoServer::Contents::CEchoContents *m_pEchoContents;
	};
}

