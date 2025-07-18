#include "MHLib/memory/CTLSMemoryPool.h"
#include "MHLib/utils/CProfileManager.h"
#include <process.h>

constexpr int THREAD_COUNT = 4;
constexpr int LOOP_COUNT = 1000;
constexpr int ALLOC_FREE_COUNT = 1000;
constexpr int DATA_SIZE = 256; // 주로 TLS Pool을 사용하는 직렬화 버퍼 크기 40 Byte

int g_ThreadCount = 3;

class TestClass
{
public:
	char data[DATA_SIZE];
};


unsigned int NewDelFunc(LPVOID lpParam)
{
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		TestClass *pTestClass[ALLOC_FREE_COUNT];
		{
			// PROFILE_BEGIN(0, "NEW");
			for (int j = 0; j < ALLOC_FREE_COUNT; j++)
			{
				PROFILE_BEGIN(0, "NEW");
				pTestClass[j] = new TestClass;
			}
		}

		{
			// PROFILE_BEGIN(0, "DEL");
			for (int j = 0; j < ALLOC_FREE_COUNT; j++)
			{
				PROFILE_BEGIN(0, "DEL");
				delete pTestClass[j];
			}
		}
	}

	return 0;
}

MHLib::memory::CTLSMemoryPoolManager<TestClass, 2048, 4, TRUE> g_TlsQueuePool;
MHLib::memory::CTLSMemoryPoolManager<TestClass, 1024, 4, FALSE> g_TlsStackPool;

unsigned int TLSPoolQueueFunc(LPVOID lpParam)
{
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		TestClass *pTestClass[ALLOC_FREE_COUNT];

		{
			// PROFILE_BEGIN(g_ThreadCount, "NEW");
			for (int j = 0; j < ALLOC_FREE_COUNT; j++)
			{
				PROFILE_BEGIN(0, "NEW");
				pTestClass[j] = g_TlsQueuePool.Alloc();
			}
		}

		{
			// PROFILE_BEGIN(g_ThreadCount, "DEL");
			for (int j = 0; j < ALLOC_FREE_COUNT; j++)
			{
				PROFILE_BEGIN(0, "DEL");
				g_TlsQueuePool.Free(pTestClass[j]);
			}
		}
	}

	return 0;
}

unsigned int TLSPoolStackFunc(LPVOID lpParam)
{
	for (int i = 0; i < LOOP_COUNT; i++)
	{
		TestClass *pTestClass[ALLOC_FREE_COUNT];

		{
			// PROFILE_BEGIN(g_ThreadCount, "NEW");
			for (int j = 0; j < ALLOC_FREE_COUNT; j++)
			{
				PROFILE_BEGIN(0, "NEW");
				pTestClass[j] = g_TlsStackPool.Alloc();
			}
		}

		{
			// PROFILE_BEGIN(g_ThreadCount, "DEL");
			for (int j = 0; j < ALLOC_FREE_COUNT; j++)
			{
				PROFILE_BEGIN(0, "DEL");
				g_TlsStackPool.Free(pTestClass[j]);
			}
		}
	}

	return 0;
}


int main()
{
	MHLib::utils::g_ProfileMgr = MHLib::utils::CProfileManager::GetInstance();


		HANDLE stdTh[THREAD_COUNT];
		for (int i = 0; i < g_ThreadCount; i++)
		{
			stdTh[i] = (HANDLE)_beginthreadex(nullptr, 0, NewDelFunc, nullptr, CREATE_SUSPENDED, 0);
			if (stdTh[i] == 0)
				return 1;
		}

		for (int i = 0; i < g_ThreadCount; i++)
		{
			ResumeThread(stdTh[i]);
		}

		WaitForMultipleObjects(g_ThreadCount, stdTh, TRUE, INFINITE);

		HANDLE lfTh[THREAD_COUNT];

		for (int i = 0; i < g_ThreadCount; i++)
		{
			lfTh[i] = (HANDLE)_beginthreadex(nullptr, 0, TLSPoolStackFunc, nullptr, CREATE_SUSPENDED, 0);
			if (lfTh[i] == 0)
				return 1;
		}

		for (int i = 0; i < g_ThreadCount; i++)
		{
			ResumeThread(lfTh[i]);
		}


		WaitForMultipleObjects(g_ThreadCount, lfTh, TRUE, INFINITE);

		MHLib::utils::g_ProfileMgr->DataOutToFileWithTag(g_ThreadCount);
		MHLib::utils::g_ProfileMgr->Clear();
	return 0;
}