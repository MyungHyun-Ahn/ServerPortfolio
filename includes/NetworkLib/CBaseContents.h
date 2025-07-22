#pragma once

namespace NetworkLib::Core::Net::Server
{
	class CNetServer;
}

namespace NetworkLib::Contents
{
	struct MOVE_JOB
	{
		UINT64 sessionId;
		// nullptr �Ѿ���� �ش� ���������� �˾Ƽ� ������ ��
		void *objectPtr;

		inline static MOVE_JOB *Alloc() noexcept
		{
			MOVE_JOB *pMoveJob = s_MoveJobPool.Alloc();
			return pMoveJob;
		}

		inline static void Free(MOVE_JOB *freeJob) noexcept
		{
			s_MoveJobPool.Free(freeJob);
		}

		inline static MHLib::memory::CTLSMemoryPoolManager<MOVE_JOB> s_MoveJobPool = MHLib::memory::CTLSMemoryPoolManager<MOVE_JOB>();

	};

	enum class RECV_RET
	{
		RECV_TRUE = 0,
		RECV_MOVE,
		RECV_FALSE
	};

	// ��� �������� BaseContent�� ��ӹ޾� ����
	class CBaseContents
	{
	public:
		friend class NetworkLib::Core::Net::Server::CNetServer;
		friend struct ContentsFrameEvent;
		friend class CMonitor;

		CBaseContents() noexcept
		{
			m_ContentsID = InterlockedIncrement(&s_CurrentContentsID);
		}

		// �������� �����Ǵ� �� -> �� �������� ������ ���ɰ� ����
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
		virtual RECV_RET OnRecv(const UINT64 sessionID, DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *message, int delayFrame) noexcept = 0;
		virtual void OnLoopEnd() noexcept = 0;

	protected:
		LONG m_ContentsID;
		inline static LONG s_CurrentContentsID = 0;

		// �� ������ ������ ������ ������ m_pTimerEvent�� flag �� ���� ������ ���� �ı�
		Task::TimerTask *m_pTimerEvent;

		// Player ���� ��û, ���� ��û ���� �Ѿ��
		MHLib::containers::CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
		MHLib::containers::CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

		LONG m_FPS = 0;

		// Key : SessionId
		// Value : �˾Ƽ� Player ��ü ���� ������Ʈ ������

		std::unordered_map<UINT64, void *> m_umapSessions;
	};
}
