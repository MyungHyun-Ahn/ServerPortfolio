#include "pch.h"
#include "CoreUtils.h"
#include "PrivateHeader/MonitoringTarget.h"
#include "CNetServer.h"
#include "CBaseContents.h"

namespace NetworkLib::Contents
{
	void CBaseContents::MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept
	{
		NetworkLib::Core::Net::Server::CNetSession *pSession
			= NetworkLib::Core::Net::Server::g_NetServer->m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & NetworkLib::Core::Net::Server::CNetSession::RELEASE_FLAG)
			== NetworkLib::Core::Net::Server::CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
			}
			return;
		}

		MOVE_JOB *pMoveJob = MOVE_JOB::Alloc();
		pMoveJob->sessionId = sessionID;
		pMoveJob->objectPtr = pObject;
		m_MoveJobQ.Enqueue(pMoveJob);
	}

	void CBaseContents::LeaveJobEnqueue(UINT64 sessionID)
	{
		m_LeaveJobQ.Enqueue(sessionID);
	}

	void CBaseContents::ProcessMoveJob() noexcept
	{
		// 이동 처리
		int moveJobQSize = m_MoveJobQ.GetUseSize();
		for (int i = 0; i < moveJobQSize; i++)
		{
			MOVE_JOB *moveJob;
			// 올린 상태로 들어올 것
			// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			m_MoveJobQ.Dequeue(&moveJob);

			// 세션 찾기
			NetworkLib::Core::Net::Server::CNetSession *pSession = NetworkLib::Core::Net::Server::g_NetServer->m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(moveJob->sessionId)];

			// 여기까진 유효한 세션일 것
			// objectPtr == nullptr이면 OnEnter 받은 쪽에서 생성
			// 자기 자신 map 에도 넣어야 함
			// 세션에 상태 설정
			// pSession->flag = m_pBaseContent::State
			OnEnter(moveJob->sessionId, moveJob->objectPtr);
			pSession->m_pCurrentContent = this;

			MOVE_JOB::s_MoveJobPool.Free(moveJob);

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
			}
		}
	}

	void CBaseContents::ProcessLeaveJob() noexcept
	{
		int leaveJobQSize = m_LeaveJobQ.GetUseSize();
		for (int i = 0; i < leaveJobQSize; i++)
		{
			UINT64 sessionID;
			m_LeaveJobQ.Dequeue(&sessionID);
			OnLeave(sessionID);
		}
	}

	void CBaseContents::ProcessRecvMsg(int delayFrame) noexcept
	{
		for (auto &it : m_umapSessions)
		{
			UINT64 sessionId = it.first;
			int sessionIdx = NetworkLib::Core::Utils::GetSessionIndex(sessionId);
			NetworkLib::Core::Net::Server::CNetSession *pSession = NetworkLib::Core::Net::Server::g_NetServer->m_arrPSessions[sessionIdx];

			InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			if ((pSession->m_iIOCountAndRelease & NetworkLib::Core::Net::Server::CNetSession::RELEASE_FLAG)
				== NetworkLib::Core::Net::Server::CNetSession::RELEASE_FLAG)
			{
				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
				{
					NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
				}
				continue;
			}

			if (sessionId != pSession->m_uiSessionID)
			{
				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
				{
					NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
				}
				continue;
			}

			LONG recvMsgCount = pSession->m_RecvMsgQueue.GetUseSize();
			for (int i = 0; i < recvMsgCount; i++)
			{
				DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *pMsg;
				{
					PROFILE_BEGIN(0, "RECV_MSQ DEQUEUE");
					pSession->m_RecvMsgQueue.Dequeue(&pMsg);
				}
				RECV_RET ret = OnRecv(sessionId, pMsg, delayFrame);
				if (ret == RECV_RET::RECV_MOVE)
				{
					if (pMsg->DecreaseRef() == 0)
						DataStructures::CSerializableBuffer<SERVER_TYPE::NET>::Free(pMsg);
					break;
				}
				if (ret == RECV_RET::RECV_FALSE)
				{
					NetworkLib::Core::Net::Server::g_NetServer->Disconnect(sessionId);

					if (pMsg->DecreaseRef() == 0)
						DataStructures::CSerializableBuffer<SERVER_TYPE::NET>::Free(pMsg);
					break;
				}

				if (pMsg->DecreaseRef() == 0)
					DataStructures::CSerializableBuffer<SERVER_TYPE::NET>::Free(pMsg);
			}

			if (recvMsgCount != 0)
				NetworkLib::Core::Net::Server::g_NetServer->SendPQCS(pSession);

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
			}
		}
	}

}