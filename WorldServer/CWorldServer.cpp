#include "pch.h"
#include "CWorldServer.h"
#include "CEntranceContents.h"
#include "WorldServerSetting.h"
#include "NetworkLib/ContentsFrameTask.h"

namespace WorldServer::Server
{
	bool CWorldServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
	{
		UNREFERENCED_PARAMETER(ip);
		UNREFERENCED_PARAMETER(port);
		return true;
	}

	void CWorldServer::OnAccept(const UINT64 sessionID) noexcept
	{
		m_pEntranceContents->MoveJobEnqueue(sessionID, nullptr);
	}

	void CWorldServer::OnClientLeave(const UINT64 sessionID) noexcept
	{
		UNREFERENCED_PARAMETER(sessionID);
	}

	void CWorldServer::OnError(int errorcode, WCHAR *errMsg) noexcept
	{
		UNREFERENCED_PARAMETER(errorcode);
		UNREFERENCED_PARAMETER(errMsg);
	}

	void CWorldServer::RegisterContentTimerEvent() noexcept
	{
		m_pEntranceContents = new WorldServer::Contents::CEntranceContents;

		NetworkLib::Task::ContentsFrameTask *pEntranceTask = new NetworkLib::Task::ContentsFrameTask;
		pEntranceTask->SetEvent(m_pEntranceContents, WorldServer::Server::Config::ENTRANCE_FPS);
		NetworkLib::Contents::CContentsThread::EnqueueEvent(pEntranceTask);
	}
}
