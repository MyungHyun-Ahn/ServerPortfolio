#include <stack>
#include "MHLib/utils/CProfileManager.h"
#include "MHLib/containers/CLFStack.h"
#include "MHLib/utils/CLockGuard.h"
#include <process.h>
#include <mutex>

std::stack<LONG> g_STDStack;
MHLib::containers::CLFStack<LONG> g_LFStack;

constexpr int THREAD_COUNT = 8;
constexpr int LOOP_COUNT = 1000;
constexpr int ALLOC_FREE_COUNT = 1000;

SRWLOCK g_srwLock;
std::mutex g_mtx;

unsigned int stdStackFunc(LPVOID lpParam)
{
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		PROFILE_BEGIN(0, "STDSTACK");
		for (int j = 0; j < ALLOC_FREE_COUNT; j++)
		{
			// MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lockGuard(g_srwLock);
			g_mtx.lock();
			g_STDStack.push(j);
			g_mtx.unlock();
		}

		for (int j = 0; j < ALLOC_FREE_COUNT; j++)
		{
			// MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lockGuard(g_srwLock);
			g_mtx.lock();
			LONG num = g_STDStack.top();
			g_STDStack.pop();
			g_mtx.unlock();

		}
	}

	return 0;
}

unsigned int lfStackFunc(LPVOID lpParam)
{
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		for (int j = 0; j < ALLOC_FREE_COUNT; j++)
		{
			PROFILE_BEGIN(0, "Push");
			g_LFStack.Push(j);
		}

		for (int j = 0; j < ALLOC_FREE_COUNT; j++)
		{
			PROFILE_BEGIN(0, "Pop");
			LONG num;
			g_LFStack.Pop(&num);
		}
	}

	return 0;
}

unsigned int lfStackLockFunc(LPVOID lpParam)
{
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		for (int j = 0; j < ALLOC_FREE_COUNT; j++)
		{
			PROFILE_BEGIN(0, "Push");
			int sum = 0;
			for (int k = 0; k < 64; k++)
			{
				sum++;
			}
			// MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lockGuard(g_srwLock);
			// g_LFStack.Push(j);
		}

		// for (int j = 0; j < ALLOC_FREE_COUNT; j++)
		// {
		// 	PROFILE_BEGIN(0, "Pop");
		// 	// MHLib::utils::CLockGuard<MHLib::utils::LOCK_TYPE::EXCLUSIVE> lockGuard(g_srwLock);
		// 	// LONG num;
		// 	// g_LFStack.Pop(&num);
		// 
		// }
	}

	return 0;
}

int main()
{
	MHLib::utils::g_ProfileMgr = MHLib::utils::CProfileManager::GetInstance();

	InitializeSRWLock(&g_srwLock);

	HANDLE stdTh[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; i++)
	{
		stdTh[i] = (HANDLE)_beginthreadex(nullptr, 0, lfStackLockFunc, nullptr, CREATE_SUSPENDED, 0);
		if (stdTh[i] == 0)
			return 1;
	}

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		ResumeThread(stdTh[i]);
	}

	WaitForMultipleObjects(THREAD_COUNT, stdTh, TRUE, INFINITE);


	// HANDLE lfTh[THREAD_COUNT];
	// 
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	lfTh[i] = (HANDLE)_beginthreadex(nullptr, 0, lfStackFunc, nullptr, CREATE_SUSPENDED, 0);
	// 	if (lfTh[i] == 0)
	// 		return 1;
	// }
	// 
	// for (int i = 0; i < THREAD_COUNT; i++)
	// {
	// 	ResumeThread(lfTh[i]);
	// }
	// 
	// 
	// WaitForMultipleObjects(THREAD_COUNT, lfTh, TRUE, INFINITE);

	MHLib::utils::g_ProfileMgr->DataOutToFile();

	return 0;
}