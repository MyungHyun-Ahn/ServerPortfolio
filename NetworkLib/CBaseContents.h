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
		// nullptr �Ѿ���� �ش� ���������� �˾Ƽ� ������ ��
		void *objectPtr;

		USE_TLS_POOL(MOVE_JOB, s_MoveJobPool)
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
		friend struct NetworkLib::Task::ContentsFrameTask;
		friend class MHLib::utils::CMonitor;

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
		virtual RECV_RET OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept = 0;
		virtual void OnLoopEnd() noexcept = 0;

		// ����͸���
		inline LONG FPSReset() noexcept { return InterlockedExchange(&m_FPS, 0); }
		inline LONG GetSessionCount() const noexcept { return m_umapSessions.size(); }

	protected:
		LONG m_ContentsID;
		inline static LONG s_CurrentContentsID = 0;

		// �� ������ ������ ������ ������ m_pTimerEvent�� flag �� ���� ������ ���� �ı�
		NetworkLib::Task::TimerTask *m_pTimerEvent;

		// Player ���� ��û, ���� ��û ���� �Ѿ��
		MHLib::containers::CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
		MHLib::containers::CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

		LONG m_FPS = 0;

		// Key : SessionId
		// Value : �˾Ƽ� Player ��ü ���� ������Ʈ ������

		std::unordered_map<UINT64, void *> m_umapSessions;
	};
}
