#pragma once

namespace NetworkLib::Core::Net::Server
{
	class CNetServer;
}

namespace NetworkLib::Task
{
	struct ContentsFrameTask;
}

namespace NetworkLib::Contents
{
	struct MOVE_JOB
	{
		UINT64 sessionId;
		// nullptr 넘어오면 해당 컨텐츠에서 알아서 만들어야 함
		void *objectPtr;

		USE_TLS_POOL(MOVE_JOB, s_MoveJobPool)
	};

	enum class RECV_RET
	{
		RECV_TRUE = 0,
		RECV_MOVE,
		RECV_FALSE
	};

	// 모든 콘텐츠는 BaseContent를 상속받아 구현
	class CBaseContents
	{
	public:
		friend class NetworkLib::Core::Net::Server::CNetServer;
		friend struct NetworkLib::Task::ContentsFrameTask;
		friend class MHLib::utils::CMonitor;

		CBaseContents() noexcept
		{
			m_ContentsID = InterlockedIncrement(&s_CurrentContentsID);
		}

		// 컨텐츠가 삭제되는 것 -> 한 프레임이 끝나고 락걸고 삭제
		~CBaseContents() noexcept
		{
			ProcessMoveJob();
			ProcessLeaveJob();
		}

		void MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept;
		void LeaveJobEnqueue(UINT64 sessionID);
		void ProcessMoveJob() noexcept;
		void ProcessLeaveJob() noexcept;
		void ProcessRecvMsg(int delayFrame) noexcept;

		virtual void OnEnter(const UINT64 sessionID, void *pObject) noexcept = 0;
		virtual void OnLeave(const UINT64 sessionID) noexcept = 0;
		virtual RECV_RET OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept = 0;
		virtual void OnLoopEnd() noexcept = 0;

		// 모니터링용
		inline LONG FPSReset() noexcept { return InterlockedExchange(&m_FPS, 0); }
		inline LONG GetSessionCount() const noexcept { return m_umapSessions.size(); }

	protected:
		LONG m_ContentsID;
		inline static LONG s_CurrentContentsID = 0;

		// 이 컨텐츠 루프를 끝내고 싶으면 m_pTimerEvent의 flag 를 끄면 루프를 돌고 파괴
		NetworkLib::Task::TimerTask *m_pTimerEvent;

		// Player 생성 요청, 삭제 요청 등이 넘어옴
		MHLib::containers::CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
		MHLib::containers::CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

		LONG m_FPS = 0;

		// Key : SessionId
		// Value : 알아서 Player 객체 등의 오브젝트 포인터

		std::unordered_map<UINT64, void *> m_umapSessions;
	};
}
