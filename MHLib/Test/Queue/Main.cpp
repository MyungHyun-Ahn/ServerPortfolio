#pragma comment(lib, "winmm.lib")
#define DISABLE_OPTIMIZATION __pragma(optimize("", off))
#define ENABLE_OPTIMIZATION  __pragma(optimize("", on))
#include <stack>
#include <queue>
#include "MHLib/utils/CProfileManager.h"
#include "MHLib/containers/CLFStack.h"
#include "MHLib/containers/CLFQueue.h"
#include "MHLib/memory/CTLSMemoryPool.h"
#include "MHLib/utils/CLockGuard.h"
#include "MHLib/containers/CDeque.h"
#include <process.h>
#include <mutex>
#include <memory>

std::queue<LONG> g_STDQueue;
MHLib::containers::CLFStack<LONG> g_LFStack;
MHLib::containers::CLFQueue<LONG> g_LFQueue;

MHLib::containers::CDeque<LONG> g_Deque;

constexpr int THREAD_COUNT = 1000;
constexpr int LOOP_COUNT = 10000;
constexpr int ALLOC_FREE_COUNT = 1000;
int g_ThreadCount = 4;

SRWLOCK g_srwLock;
std::mutex g_mtx;

constexpr int ENQUEUE_COUNT = 1;
alignas(64)
LONG g_EnqueueTPS = 0;
alignas(64)
LONG g_DequeueTPS = 0;
alignas(64)
LONG g_TPS = 0;

BOOL g_Running = TRUE;

int slpCnt = 1;
int isLock = 1;

unsigned int stdQueueProvider(LPVOID lpParam)
{
	// 잡큐 Enqueue
	while (g_Running)
	{
		for (int i = 0; i < ENQUEUE_COUNT; i++)
		{
			{
				PROFILE_BEGIN(0, "ENQUEUE");
				MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lockGuard(g_srwLock);
				g_Deque.push_back(i);
			}
			InterlockedIncrement(&g_EnqueueTPS);

			for (int slp = 0; slp < slpCnt; slp++)
			{
				Sleep(0);
			}
		}
	}

	return 0;
}

unsigned int stdQueueConsumer(LPVOID lpParam)
{
	// 잡큐 소비 - 25프레임
	DWORD mTime = timeGetTime();
	while (g_Running)
	{
		DWORD dTime = timeGetTime() - mTime;
		if (dTime < 40)
			Sleep(40 - dTime);

		int enqSize = g_Deque.size();

		for (int i = 0; i < enqSize; i++)
		{
			// Dequeue
			{
				PROFILE_BEGIN(0, "DEQUEUE");
				MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lockGuard(g_srwLock);
				LONG deqNum = g_Deque.front();
				g_Deque.pop_front();
			}
			InterlockedIncrement(&g_DequeueTPS);

			// Sleep(0);
		}

		InterlockedIncrement(&g_TPS);
		mTime += 40;
	}

	return 0;
}

unsigned int lfQueueProvider(LPVOID lpParam)
{
	// 잡큐 Enqueue
	while (g_Running)
	{
		for (int i = 0; i < ENQUEUE_COUNT; i++)
		{
			{
				PROFILE_BEGIN(0, "ENQUEUE");
				g_LFQueue.Enqueue(i);
			}
			InterlockedIncrement(&g_EnqueueTPS);
			
			for (int slp = 0; slp < slpCnt; slp++)
			{
				Sleep(0);
			}
		}
	}

	return 0;
}

unsigned int lfQueueConsumer(LPVOID lpParam)
{
	// 잡큐 소비 - 25프레임
	DWORD mTime = timeGetTime();
	while (g_Running)
	{
		DWORD dTime = timeGetTime() - mTime;
		if (dTime < 40)
			Sleep(40 - dTime);

		int enqSize = g_LFQueue.GetUseSize();

		for (int i = 0; i < enqSize; i++)
		{
			// Dequeue
			{
				PROFILE_BEGIN(0, "DEQUEUE");
				LONG deqNum;
				g_LFQueue.Dequeue(&deqNum);
			}
			InterlockedIncrement(&g_DequeueTPS);

			// Job Queue Dequeue 하고 특정 일을 함을 가정
			// Sleep(0);
		}

		InterlockedIncrement(&g_TPS);
		mTime += 40;
	}

	return 0;
}

unsigned int MonitoringThread(LPVOID lpParam)
{
	DWORD mTime = timeGetTime();

	LONG loopCount = 0; // 3분

	while (g_Running)
	{
		DWORD dTime = timeGetTime() - mTime;
		if (dTime < 1000)
			Sleep(1000 - dTime);
		loopCount++;

		// Monitoring
		LONG enqueueTPS = InterlockedExchange(&g_EnqueueTPS, 0);
		LONG dequeueTPS = InterlockedExchange(&g_DequeueTPS, 0);
		LONG TPS = InterlockedExchange(&g_TPS, 0);

		printf("TH_CNT %d, TPS : %d\n", g_ThreadCount, TPS);
		printf("ENQUEUE_TPS : %d\n", enqueueTPS);
		printf("DEQUEUE_TPS : %d\n", dequeueTPS);

		if (loopCount == 60 * 10)
		{
			if (isLock)
				MHLib::utils::g_ProfileMgr->DataOutToFileWithTag(dequeueTPS, "Lock");
			else
				MHLib::utils::g_ProfileMgr->DataOutToFileWithTag(dequeueTPS, "LockFree");

			g_Running = FALSE;
		}
		mTime += 1000;
	}

	return 0;
}

#include <list>

int main()
{
	{
		std::shared_ptr<INT> a(new INT);
	}

	std::vector<INT> vec;
	auto it = vec.begin();
	it += 5;

	std::list<INT> li;
	auto it2 = li.begin();
	it2 += 5;

	MHLib::utils::g_ProfileMgr = MHLib::utils::CProfileManager::GetInstance();
	timeBeginPeriod(1);
	InitializeSRWLock(&g_srwLock);

	for (; g_ThreadCount <= 4; g_ThreadCount *= 2)
	{
			{
				HANDLE stdTh[THREAD_COUNT]; // 워커 개수
				for (int i = 0; i < g_ThreadCount; i++)
				{
					stdTh[i] = (HANDLE)_beginthreadex(nullptr, 0, stdQueueProvider, nullptr, CREATE_SUSPENDED, 0);
					if (stdTh[i] == 0)
						return 1;
				}

				HANDLE consumerThread = (HANDLE)_beginthreadex(nullptr, 0, stdQueueConsumer, nullptr, CREATE_SUSPENDED, 0);
				if (consumerThread == 0)
					return 1;

				HANDLE monitorTh = (HANDLE)_beginthreadex(nullptr, 0, MonitoringThread, nullptr, CREATE_SUSPENDED, 0);
				if (monitorTh == 0)
					return 1;

				for (int i = 0; i < g_ThreadCount; i++)
				{
					ResumeThread(stdTh[i]);
				}

				ResumeThread(consumerThread);
				ResumeThread(monitorTh);

				WaitForMultipleObjects(g_ThreadCount, stdTh, TRUE, INFINITE);
				WaitForSingleObject(consumerThread, INFINITE);
				WaitForSingleObject(monitorTh, INFINITE);
			}

			g_Running = TRUE;
			isLock = 0;
			MHLib::utils::g_ProfileMgr->Clear();

			{
				HANDLE stdTh[THREAD_COUNT]; // 워커 개수
				for (int i = 0; i < g_ThreadCount; i++)
				{
					stdTh[i] = (HANDLE)_beginthreadex(nullptr, 0, lfQueueProvider, nullptr, CREATE_SUSPENDED, 0);
					if (stdTh[i] == 0)
						return 1;
				}

				HANDLE consumerThread = (HANDLE)_beginthreadex(nullptr, 0, lfQueueConsumer, nullptr, CREATE_SUSPENDED, 0);
				if (consumerThread == 0)
					return 1;

				HANDLE monitorTh = (HANDLE)_beginthreadex(nullptr, 0, MonitoringThread, nullptr, CREATE_SUSPENDED, 0);
				if (monitorTh == 0)
					return 1;

				for (int i = 0; i < g_ThreadCount; i++)
				{
					ResumeThread(stdTh[i]);
				}

				ResumeThread(consumerThread);
				ResumeThread(monitorTh);

				WaitForMultipleObjects(g_ThreadCount, stdTh, TRUE, INFINITE);
				WaitForSingleObject(consumerThread, INFINITE);
				WaitForSingleObject(monitorTh, INFINITE);
			}

			g_Running = TRUE;
			isLock = 1;
			MHLib::utils::g_ProfileMgr->Clear();
	}

	return 0;
}