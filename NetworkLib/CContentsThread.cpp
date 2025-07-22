#include "pch.h"
#include "CContentsThread.h"
#include "PrivateHeader/MonitoringTarget.h"
#include "NetSetting.h"

namespace NetworkLib::Contents
{
	CContentsThread::CContentsThread()
	{
		InitializeSRWLock(&m_lockTimerEventQ);
	}

	CContentsThread::~CContentsThread()
	{
		CloseHandle(m_ThreadHandle);
	}

	bool CContentsThread::Create() noexcept
	{
		m_RunningFlag = TRUE;
		m_ThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, &CContentsThread::ThreadFunc, this, CREATE_SUSPENDED, nullptr);
		s_arrContentsThreads.push_back(this);
		return m_ThreadHandle != nullptr;
	}

	void CContentsThread::Resume() noexcept
	{
		if (m_ThreadHandle != INVALID_HANDLE_VALUE)
		{
			ResumeThread(m_ThreadHandle);
		}
	}

	void CContentsThread::RunAll() noexcept
	{
		for (CContentsThread *thread : s_arrContentsThreads)
		{
			if (thread != nullptr && thread->m_ThreadHandle != INVALID_HANDLE_VALUE)
			{
				thread->Resume();
			}
		}
	}

	void CContentsThread::EnqueueEvent(Task::BaseTask *pTask)
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

	void CContentsThread::EnqueueEventMy(Task::BaseTask *pTask) noexcept
	{
		m_EnqueueEventQ.Enqueue(pTask);
		InterlockedExchange(&m_EnqueueFlag, TRUE);
		WakeByAddressSingle(&m_EnqueueFlag);
	}

	unsigned __stdcall CContentsThread::ThreadFunc(void *pThis) noexcept
	{
		CContentsThread *pThread = static_cast<CContentsThread *>(pThis);
		pThread->Run();
		return 0;
	}

	void CContentsThread::CheckEnqueueEventQ() noexcept
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

	bool CContentsThread::CheckTimerEventQ() noexcept
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

	bool CContentsThread::WorkStealing() noexcept
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

				InterlockedIncrement(&s_monitoringTargets->workStealingCount);

				return true;
			}
			ReleaseSRWLockExclusive(&thread->m_lockTimerEventQ);
		}

		return false;
	}

	bool CContentsThread::DelegateWork() noexcept
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
		InterlockedIncrement(&s_monitoringTargets->delegateWorkCount);
		return true;
	}

	void CContentsThread::ProcessTimerEvent() noexcept
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
		pTimerTask->m_nextExecuteTime += pTimerTask->m_timeMs * delayFrame;

		if ((dTime * -1) >
			((1000 / NetworkLib::Core::Net::Server::MAX_CONTENTS_FPS) * NetworkLib::Core::Net::Server::DELAY_FRAME))
		{
			DelegateWork();
		}

		MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
		m_TimerEventQueue.push(pTimerTask);

		return;
	}

	void CContentsThread::Run() noexcept
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




}