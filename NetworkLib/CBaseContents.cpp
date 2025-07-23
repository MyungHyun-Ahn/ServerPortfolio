#include "pch.h"
#include "CNetServer.h"
#include "CBaseContents.h"

namespace NetworkLib::Contents
{
	void CBaseContents::MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept
	{
		CNetSession *pSession
			= g_NetServer->m_arrPSessions[GetSessionIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG)
			== CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				g_NetServer->ReleaseSession(pSession);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				g_NetServer->ReleaseSession(pSession);
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
		// �̵� ó��
		int moveJobQSize = m_MoveJobQ.GetUseSize();
		for (int i = 0; i < moveJobQSize; i++)
		{
			MOVE_JOB *moveJob;
			// �ø� ���·� ���� ��
			// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			m_MoveJobQ.Dequeue(&moveJob);

			// ���� ã��
			CNetSession *pSession = g_NetServer->m_arrPSessions[GetSessionIndex(moveJob->sessionId)];

			// ������� ��ȿ�� ������ ��
			// objectPtr == nullptr�̸� OnEnter ���� �ʿ��� ����
			// �ڱ� �ڽ� map ���� �־�� ��
			// ���ǿ� ���� ����
			// pSession->flag = m_pBaseContent::State
			OnEnter(moveJob->sessionId, moveJob->objectPtr);
			pSession->m_pCurrentContent = this;

			MOVE_JOB::s_MoveJobPool.Free(moveJob);

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				g_NetServer->ReleaseSession(pSession);
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
			int sessionIdx = GetSessionIndex(sessionId);
			CNetSession *pSession = g_NetServer->m_arrPSessions[sessionIdx];

			InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG)
				== CNetSession::RELEASE_FLAG)
			{
				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
				{
					g_NetServer->ReleaseSession(pSession);
				}
				continue;
			}

			if (sessionId != pSession->m_uiSessionID)
			{
				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
				{
					g_NetServer->ReleaseSession(pSession);
				}
				continue;
			}

			LONG recvMsgCount = pSession->m_RecvMsgQueue.GetUseSize();
			for (int i = 0; i < recvMsgCount; i++)
			{
				CSerializableBuffer<SERVER_TYPE::NET> *pMsg;
				{
					PROFILE_BEGIN(0, "RECV_MSQ DEQUEUE");
					pSession->m_RecvMsgQueue.Dequeue(&pMsg);
				}
				RECV_RET ret = OnRecv(sessionId, pMsg, delayFrame);
				if (ret == RECV_RET::RECV_MOVE)
				{
					if (pMsg->DecreaseRef() == 0)
						CSerializableBuffer<SERVER_TYPE::NET>::Free(pMsg);
					break;
				}
				if (ret == RECV_RET::RECV_FALSE)
				{
					g_NetServer->Disconnect(sessionId);

					if (pMsg->DecreaseRef() == 0)
						CSerializableBuffer<SERVER_TYPE::NET>::Free(pMsg);
					break;
				}

				if (pMsg->DecreaseRef() == 0)
					CSerializableBuffer<SERVER_TYPE::NET>::Free(pMsg);
			}

			if (recvMsgCount != 0)
				g_NetServer->SendPQCS(pSession);

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				g_NetServer->ReleaseSession(pSession);
			}
		}
	}

}