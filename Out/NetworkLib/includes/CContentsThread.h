#pragma once

namespace NetworkLib::Core::Net::Server
{
	class CNetServer;
}

namespace NetworkLib::Core::Utils
{
	class CAccessor;
}

namespace NetworkLib::Contents
{

	class CContentsThread
	{
	public:
		friend class NetworkLib::Core::Net::Server::CNetServer;
		friend class NetworkLib::Core::Utils::CAccessor;

		CContentsThread() { InitializeSRWLock(&m_lockTimerEventQ); }
		~CContentsThread() { CloseHandle(m_ThreadHandle); }

		bool Create() noexcept
		{
			m_RunningFlag = TRUE;
			m_ThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, &CContentsThread::ThreadFunc, this, CREATE_SUSPENDED, nullptr);
			s_arrContentsThreads.push_back(this);
			return m_ThreadHandle != nullptr;
		}

		void Resume() noexcept
		{
			if (m_ThreadHandle != INVALID_HANDLE_VALUE)
			{
				ResumeThread(m_ThreadHandle);
			}
		}

		static void RunAll() noexcept
		{
			for (CContentsThread *thread : s_arrContentsThreads)
			{
				if (thread != nullptr && thread->m_ThreadHandle != INVALID_HANDLE_VALUE)
				{
					thread->Resume();
				}
			}
		}

		static void EnqueueEvent(Task::BaseTask *pTask)
		{
			CContentsThread *targetThread = nullptr;
			DWORD maxDTime = 0;
			for (CContentsThread *thread : s_arrContentsThreads)
			{
				if (thread->m_PrevSleepTime >= maxDTime)
				{
					maxDTime = thread->m_PrevSleepTime;
					targetThread = thread;
				}
			}
			targetThread->EnqueueEventMy(pTask);
		}

		// 자기 자신 스레드에게 Enqueue Event
		void EnqueueEventMy(Task::BaseTask *pTask)
		{
			m_EnqueueEventQ.Enqueue(pTask);
			InterlockedExchange(&m_EnqueueFlag, TRUE);
			WakeByAddressSingle(&m_EnqueueFlag);
		}

	private:
		static unsigned __stdcall ThreadFunc(void *pThis) noexcept
		{
			CContentsThread *pThread = static_cast<CContentsThread *>(pThis);
			pThread->Run();
			return 0;
		}

		// Enqueue 이벤트 큐를 확인하고 바로 수행가능한 이벤트면 수행
		// 타이머 이벤트면 타이머 이벤트 큐에 인큐
		void CheckEnqueueEventQ() noexcept
		{
			LONG eventQSize = m_EnqueueEventQ.GetUseSize();
			if (eventQSize != 0)
			{
				for (int i = 0; i < eventQSize; i++)
				{
					Task::BaseTask *pTask;
					// 꺼내고
					m_EnqueueEventQ.Dequeue(&pTask);

					if (pTask->m_isTimerTask)
					{
						MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
						m_TimerEventQueue.push(static_cast<Task::TimerTask *>(pTask));
					}
					else
					{
						// 일회성 이벤트는 바로 실행하고
						pTask->execute(1);
						delete pTask;
					}
				}
			}
		}

		bool CheckTimerEventQ() noexcept
		{
			if (m_TimerEventQueue.empty())
			{
				InterlockedExchange(&m_EnqueueFlag, FALSE);
				if (!WorkStealing())
				{
					m_PrevSleepTime = INFINITE;
					WaitOnAddress(&m_EnqueueFlag, &m_EnqueueComparand
						, sizeof(LONG), INFINITE);
				}
				return false;
			}
			return true;
		}

		// 할일이 없는 경우 WaitOnAddress로 빠지기 전에 수행
		// 가져온 일이 있으면 WaitOnAddress로 빠지지 않고 자기 자신의 일로 만들고 수행됨

		bool WorkStealing() noexcept
		{
			for (CContentsThread *thread : s_arrContentsThreads)
			{
				if (thread == this)
					continue;

				AcquireSRWLockExclusive(&thread->m_lockTimerEventQ);
				if (thread->m_TimerEventQueue.size() != 0)
				{
					Task::TimerTask *pTimerTask = thread->m_TimerEventQueue.top();
					thread->m_TimerEventQueue.pop();
					ReleaseSRWLockExclusive(&thread->m_lockTimerEventQ);

					AcquireSRWLockExclusive(&m_lockTimerEventQ);
					m_TimerEventQueue.push(pTimerTask);
					ReleaseSRWLockExclusive(&m_lockTimerEventQ);

					// InterlockedIncrement(&g_monitor.m_StealWork);

					return true;
				}
				ReleaseSRWLockExclusive(&thread->m_lockTimerEventQ);
			}

			return false;
		}

		// 자기 자신이 바쁠 때 다른 스레드로 일을 넘겨주는 함수

		bool DelegateWork() noexcept
		{
			Task::TimerTask *pTimerTask;
			{
				MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
				if (m_TimerEventQueue.empty())
				{
					return false;
				}

				pTimerTask = m_TimerEventQueue.top();
				m_TimerEventQueue.pop();
			}

			EnqueueEvent(pTimerTask);
			// InterlockedIncrement(&g_monitor.m_DelWork);
			return true;
		}

		void ProcessTimerEvent() noexcept
		{
			Task::TimerTask *pTimerTask;
			LONG dTime;
			{
				MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);

				if (m_TimerEventQueue.empty())
				{
					return;
				}

				pTimerTask = m_TimerEventQueue.top();
				dTime = pTimerTask->m_nextExecuteTime - timeGetTime();
				if (dTime > 0)
				{
					m_PrevSleepTime = dTime;
					Sleep(dTime);
					return;
				}

				m_TimerEventQueue.pop();
			}

			// 종료해야할 이벤트면
			if (!pTimerTask->m_isRunning)
			{
				delete pTimerTask;
				return;
			}

			INT delayFrame;
			if (pTimerTask->m_timeMs == 0)
				delayFrame = 0;
			else
				delayFrame = ((dTime * -1) / pTimerTask->m_timeMs) + 1;

			pTimerTask->execute(delayFrame);
			pTimerTask->m_nextExecuteTime+= pTimerTask->m_timeMs * delayFrame;

			if ((dTime * -1) >
				((1000 / NetworkLib::Core::Net::Server::MAX_CONTENTS_FPS) * NetworkLib::Core::Net::Server::DELAY_FRAME))
			{
				DelegateWork();
			}

			MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
			m_TimerEventQueue.push(pTimerTask);

			return;
		}

		void Run() noexcept
		{
			// SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

			while (m_RunningFlag)
			{
				CheckEnqueueEventQ();

				if (!CheckTimerEventQ())
					continue;

				ProcessTimerEvent();
			}
		}

	private:
		HANDLE					m_ThreadHandle = INVALID_HANDLE_VALUE;
		BOOL					m_RunningFlag = FALSE;

		MHLib::containers::CLFQueue<Task::BaseTask *>	m_EnqueueEventQ;
		LONG					m_EnqueueFlag = FALSE;
		LONG					m_EnqueueComparand = FALSE;

		// TimerEvent Queue
		std::priority_queue<Task::TimerTask *, std::vector<Task::TimerTask *>, Task::TimerTaskComparator>	m_TimerEventQueue;
		SRWLOCK					m_lockTimerEventQ;

		// SleepTime이 모자르면 다른 스레드에 일감 넘김
		DWORD					m_PrevSleepTime = INFINITE;

		inline static NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets s_monitoringTargets;
		inline static std::vector<CContentsThread *> s_arrContentsThreads;
	};
}