#include "pch.h"
#include "CNetServer.h"
#include "CBaseContents.h"

namespace NetworkLib::Contents
{
	void CBaseContents::MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept
	{
		NetworkLib::Core::Net::Server::CNetSession *pSession
			= NetworkLib::Core::Net::Server::g_NetServer->AcquireSession(sessionID);
		if (pSession == nullptr)
		{
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
		auto releaseSession = [](NetworkLib::Core::Net::Server::CNetSession *pSession) noexcept
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
			}
		};

		// 이동 처리
		int moveJobQSize = m_MoveJobQ.GetUseSize();
		for (int i = 0; i < moveJobQSize; i++)
		{
			MOVE_JOB *moveJob;
			// 올린 상태로 들어올 것
			// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			m_MoveJobQ.Dequeue(&moveJob);

			// 세션 찾기
			NetworkLib::Core::Net::Server::CNetSession *pSession 
				= NetworkLib::Core::Net::Server::g_NetServer->AcquireSession(moveJob->sessionId);
			if (pSession == nullptr)
			{
				MOVE_JOB::Free(moveJob);
				continue;
			}

			// 여기까진 유효한 세션일 것
			// objectPtr == nullptr이면 OnEnter 받은 쪽에서 생성
			// 자기 자신 map 에도 넣어야 함
			// 세션에 상태 설정
			// pSession->flag = m_pBaseContent::State
			OnEnter(moveJob->sessionId, moveJob->objectPtr);
			pSession->RegisterContents(this);

			MOVE_JOB::Free(moveJob);
			releaseSession(pSession);
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
		auto releaseSession = [](NetworkLib::Core::Net::Server::CNetSession *pSession) noexcept
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NetworkLib::Core::Net::Server::g_NetServer->ReleaseSession(pSession);
			}
		};
		auto freeRecvMessage = [](NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pMsg) noexcept
		{
			if (pMsg->DecreaseRef() == 0)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Free(pMsg);
			}
		};

		for (auto &it : m_umapSessions)
		{
			UINT64 sessionId = it.first;
			NetworkLib::Core::Net::Server::CNetSession *pSession 
				= NetworkLib::Core::Net::Server::g_NetServer->AcquireSession(sessionId);
			if (pSession == nullptr)
			{
				continue;
			}

			LONG recvMsgCount = pSession->m_RecvMsgQueue.GetUseSize();
			for (int i = 0; i < recvMsgCount; i++)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pMsg;
				{
					PROFILE_BEGIN(0, "RECV_MSQ DEQUEUE");
					pSession->m_RecvMsgQueue.Dequeue(&pMsg);
				}
				RECV_RET ret = OnRecv(sessionId, pMsg, delayFrame);
				if (ret == RECV_RET::RECV_MOVE)
				{
					freeRecvMessage(pMsg);
					break;
				}
				if (ret == RECV_RET::RECV_FALSE)
				{
					NetworkLib::Core::Net::Server::g_NetServer->Disconnect(sessionId);
					freeRecvMessage(pMsg);
					break;
				}

				freeRecvMessage(pMsg);
			}

			if (recvMsgCount != 0)
				NetworkLib::Core::Net::Server::g_NetServer->SendPQCS(pSession);

			releaseSession(pSession);
		}
	}

}
