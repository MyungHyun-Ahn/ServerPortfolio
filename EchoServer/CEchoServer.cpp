#include "pch.h"
#include "CPlayer.h"
#include "CAuthContents.h"
#include "CEchoContents.h"
#include "CEchoServer.h"
#include "NetworkLib/ContentsFrameTask.h"
#include "EchoServerSetting.h"

namespace EchoServer::Server
{
	bool CEchoServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
	{
		return true;
	}
	void CEchoServer::OnAccept(const UINT64 sessionID) noexcept
	{
		m_pAuthContents->MoveJobEnqueue(sessionID, nullptr);
	}

	// 딱히 할일이 없음
	void CEchoServer::OnClientLeave(const UINT64 sessionID) noexcept
	{
	}

	// 미사용
	void CEchoServer::OnError(int errorcode, WCHAR *errMsg) noexcept
	{
	}


	void CEchoServer::RegisterContentTimerEvent() noexcept
	{
		m_pAuthContents = new EchoServer::Contents::CAuthContents;
		NetworkLib::Task::ContentsFrameTask *pAuthTask = new NetworkLib::Task::ContentsFrameTask;
		pAuthTask->SetEvent(m_pAuthContents, EchoServer::Server::AUTH_FPS);
		NetworkLib::Contents::CContentsThread::EnqueueEvent(pAuthTask);

		m_pEchoContents = new EchoServer::Contents::CEchoContents;
		NetworkLib::Task::ContentsFrameTask *pEchoTask = new NetworkLib::Task::ContentsFrameTask;
		pEchoTask->SetEvent(m_pEchoContents, EchoServer::Server::ECHO_FPS);
		NetworkLib::Contents::CContentsThread::EnqueueEvent(pEchoTask);

	}
}