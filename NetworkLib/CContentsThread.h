#pragma once

namespace NetworkLib::Core::Net::Server
{
	class CNetServer;
}

namespace NetworkLib::Core::Utils
{
	class CAccessor;
}

namespace NetworkLib::Core::Monitoring
{
	class ContentsThreadMonitoringTargets;
}

namespace NetworkLib::Contents
{
	class CContentsThread
	{
	public:
		friend class NetworkLib::Core::Net::Server::CNetServer;
		friend class NetworkLib::Core::Utils::CAccessor;

		CContentsThread();
		~CContentsThread();

		bool Create() noexcept;
		void Resume() noexcept;

		static void RunAll() noexcept;
		static void EnqueueEvent(NetworkLib::Task::BaseTask *pTask);

		// �ڱ� �ڽ� �����忡�� Enqueue Event
		void EnqueueEventMy(NetworkLib::Task::BaseTask *pTask) noexcept;

	private:
		static unsigned __stdcall ThreadFunc(void *pThis) noexcept;

		// Enqueue �̺�Ʈ ť�� Ȯ���ϰ� �ٷ� ���డ���� �̺�Ʈ�� ����
		// Ÿ�̸� �̺�Ʈ�� Ÿ�̸� �̺�Ʈ ť�� ��ť
		void CheckEnqueueEventQ() noexcept;
		bool CheckTimerEventQ() noexcept;

		// ������ ���� ��� WaitOnAddress�� ������ ���� ����
		// ������ ���� ������ WaitOnAddress�� ������ �ʰ� �ڱ� �ڽ��� �Ϸ� ����� �����
		bool WorkStealing() noexcept;

		// �ڱ� �ڽ��� �ٻ� �� �ٸ� ������� ���� �Ѱ��ִ� �Լ�
		bool DelegateWork() noexcept;
		void ProcessTimerEvent() noexcept;
		void Run() noexcept;

	private:
		HANDLE					m_ThreadHandle = INVALID_HANDLE_VALUE;
		BOOL					m_RunningFlag = FALSE;

		MHLib::containers::CLFQueue<NetworkLib::Task::BaseTask *>	m_EnqueueEventQ;
		LONG					m_EnqueueFlag = FALSE;
		LONG					m_EnqueueComparand = FALSE;

		// TimerEvent Queue
		std::priority_queue<NetworkLib::Task::TimerTask *, std::vector<NetworkLib::Task::TimerTask *>, NetworkLib::Task::TimerTaskComparator>	m_TimerEventQueue;
		SRWLOCK					m_lockTimerEventQ;

		// SleepTime�� ���ڸ��� �ٸ� �����忡 �ϰ� �ѱ�
		DWORD					m_PrevSleepTime = INFINITE;

		inline static NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets *s_monitoringTargets;
		inline static std::vector<CContentsThread *> s_arrContentsThreads;
	};
}