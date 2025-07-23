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

		// 자기 자신 스레드에게 Enqueue Event
		void EnqueueEventMy(NetworkLib::Task::BaseTask *pTask) noexcept;

	private:
		static unsigned __stdcall ThreadFunc(void *pThis) noexcept;

		// Enqueue 이벤트 큐를 확인하고 바로 수행가능한 이벤트면 수행
		// 타이머 이벤트면 타이머 이벤트 큐에 인큐
		void CheckEnqueueEventQ() noexcept;
		bool CheckTimerEventQ() noexcept;

		// 할일이 없는 경우 WaitOnAddress로 빠지기 전에 수행
		// 가져온 일이 있으면 WaitOnAddress로 빠지지 않고 자기 자신의 일로 만들고 수행됨
		bool WorkStealing() noexcept;

		// 자기 자신이 바쁠 때 다른 스레드로 일을 넘겨주는 함수
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

		// SleepTime이 모자르면 다른 스레드에 일감 넘김
		DWORD					m_PrevSleepTime = INFINITE;

		inline static NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets *s_monitoringTargets;
		inline static std::vector<CContentsThread *> s_arrContentsThreads;
	};
}