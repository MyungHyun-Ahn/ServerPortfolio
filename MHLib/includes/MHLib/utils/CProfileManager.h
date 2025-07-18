#pragma once

#include <windows.h>
#include <map>
#include <chrono>

#include "DefineSingleton.h"

namespace MHLib::utils
{

#pragma warning(disable : 26495)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)

#define PROFILE_BEGIN(num, tagName)											\
				int tlsIdx_##num = (int)TlsGetValue(MHLib::utils::g_ProfileMgr->m_dwInfosTlsIdx); \
				if (tlsIdx_##num == 0)	\
				{	\
					tlsIdx_##num = MHLib::utils::g_ProfileMgr->AllocThreadInfo(); \
					TlsSetValue(MHLib::utils::g_ProfileMgr->m_dwInfosTlsIdx, (LPVOID)tlsIdx_##num); \
				}	\
				MHLib::utils::stTlsProfileInfo *tlsInfo_##num = MHLib::utils::g_ProfileMgr->m_arrProfileInfos[tlsIdx_##num]; \
				thread_local static int profileIdx_##num = tlsInfo_##num->currentProfileSize++; \
				MHLib::utils::CProfile profile_##num(profileIdx_##num, __FUNCSIG__, tagName)

	struct stPROFILE
	{
		stPROFILE()
		{
			for (int i = 0; i < MINMAX_REMOVE_COUNT; i++)
			{
				iMaxTime[i] = 0;
				iMinTime[i] = INT64_MAX;
			}
			szName[0] = '\0';
		}

		char				szName[300]; // �̸�
		__int64				iTotalTime = 0; // �� ���� �ð�
		// �ִ� �ּ� ���� �ŷڵ��� ������ �� �����Ƿ� ���ġ���� ������ ����
		static constexpr int MINMAX_REMOVE_COUNT = 2;
		__int64				iMinTime[MINMAX_REMOVE_COUNT]; // �ִ� ���� �ð�
		__int64				iMaxTime[MINMAX_REMOVE_COUNT]; // �ּ� ���� �ð�
		__int64				iCall = 0; // ȣ�� Ƚ��
		bool				isUsed = false;
	};

	// ������ ���� 1����
	struct stTlsProfileInfo
	{
		DWORD threadId = GetCurrentThreadId();
		stPROFILE *arrProfile;
		int currentProfileSize = 0;
		static constexpr int MAX_PROFILE_ARR_SIZE = 100;

		// Data out�� Reset�� ����� Ű �Է����� ����ǹǷ�
		// �� ������ ����ȭ ������ ����ų Ȯ���� �ſ� ����
		LONG resetFlag = FALSE;

		void Init() noexcept
		{
			arrProfile = new(std::nothrow) stPROFILE[MAX_PROFILE_ARR_SIZE];
		}
		void Reset() noexcept
		{
			for (int i = 0; i < currentProfileSize; i++)
			{
				arrProfile[i].iCall = 0;
				arrProfile[i].iTotalTime = 0;

				for (int j = 0; j < stPROFILE::MINMAX_REMOVE_COUNT; j++)
				{
					arrProfile[i].iMinTime[j] = INT64_MAX;
					arrProfile[i].iMaxTime[j] = 0;
				}
			}
		}
		void Clear() noexcept
		{
			delete[] arrProfile;
		}
	};

	class CProfileManager : public Singleton<CProfileManager>
	{
		friend class Singleton<CProfileManager>;
		friend class CProfile;

	private:
		CProfileManager()
		{
			QueryPerformanceFrequency(&m_lFreq);

			m_dwInfosTlsIdx = TlsAlloc();
			if (m_dwInfosTlsIdx == TLS_OUT_OF_INDEXES)
			{
				DWORD err = GetLastError();
				throw err;
			}
		}
		~CProfileManager()
		{
			// DataOutToFile();
			Clear();
			TlsFree(m_dwInfosTlsIdx);
		}

	public:
		// �������� ���� ������ ����ϰ� ������
		// �� �Լ��� ȣ���Ѵ�.
		void DataOutToFile()
		{
			CHAR fileName[MAX_PATH];

			__time64_t time64 = _time64(nullptr);
			tm nowTm;
			errno_t err1 = localtime_s(&nowTm, &time64);
			if (err1 != 0)
			{
				throw;
			}

			CreateDirectory(L"PROFILE", NULL);

			sprintf_s(fileName, "PROFILE\\Profile_%04d%02d%02d_%02d%02d%02d.profile"
				, nowTm.tm_year + 1900
				, nowTm.tm_mon
				, nowTm.tm_mday
				, nowTm.tm_hour
				, nowTm.tm_min
				, nowTm.tm_sec);


			FILE *fp;
			errno_t err2 = fopen_s(&fp, fileName, "w");
			if (err2 != 0 || fp == nullptr)
				throw err2;

			for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
			{
				fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
				fprintf_s(fp, "%-15s | %-100s | %-15s | %-15s | %-15s | %-15s | %-15s |\n", "ThreadId", "Name", "TotalTime(ns)", "Average(ns)", "Min(ns)", "Max(ns)", "Call");
				fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

				stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

				for (int profileIdx = 0; profileIdx < pInfo->currentProfileSize; profileIdx++)
				{
					stPROFILE *pProfile = &pInfo->arrProfile[profileIdx];
					if (pProfile->iCall < 4)
						continue;

					__int64 totalTime = pProfile->iTotalTime;
					totalTime -= pProfile->iMaxTime[0];
					totalTime -= pProfile->iMaxTime[1];
					totalTime -= pProfile->iMinTime[0];
					totalTime -= pProfile->iMinTime[1];

					if (pProfile->iCall - 4 > 0)
					{
						__int64 average = totalTime / (pProfile->iCall - 4);

						fprintf_s(fp, "%-15d | %-100s | %-15lld | %-15lld | %-15lld | %-15lld | %-15lld |\n"
							, pInfo->threadId
							, pProfile->szName
							, totalTime
							, average
							, pProfile->iMinTime[0]
							, pProfile->iMaxTime[0]
							, pProfile->iCall - 4);
					}
				}

				fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
			}

			fclose(fp);
		}

		void DataOutToFileWithTag(LONG tagNum, const char *tagStr = nullptr)
		{
			CHAR fileName[MAX_PATH];

			__time64_t time64 = _time64(nullptr);
			tm nowTm;
			errno_t err1 = localtime_s(&nowTm, &time64);
			if (err1 != 0)
			{
				throw;
			}

			CreateDirectory(L"PROFILE", NULL);

			if (tagStr == nullptr)
				sprintf_s(fileName, "PROFILE\\Profile_%04d%02d%02d_%02d%02d%02d_%02d.profile"
					, nowTm.tm_year + 1900
					, nowTm.tm_mon
					, nowTm.tm_mday
					, nowTm.tm_hour
					, nowTm.tm_min
					, nowTm.tm_sec
					, tagNum);
			else
				sprintf_s(fileName, "PROFILE\\Profile_%04d%02d%02d_%02d%02d%02d_%02d_%s.profile"
					, nowTm.tm_year + 1900
					, nowTm.tm_mon
					, nowTm.tm_mday
					, nowTm.tm_hour
					, nowTm.tm_min
					, nowTm.tm_sec
					, tagNum
					, tagStr);


			FILE *fp;
			errno_t err2 = fopen_s(&fp, fileName, "w");
			if (err2 != 0 || fp == nullptr)
				throw err2;

			for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
			{
				fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
				fprintf_s(fp, "%-15s | %-100s | %-15s | %-15s | %-15s | %-15s | %-15s |\n", "ThreadId", "Name", "TotalTime(ns)", "Average(ns)", "Min(ns)", "Max(ns)", "Call");
				fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

				stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

				for (int profileIdx = 0; profileIdx < pInfo->currentProfileSize; profileIdx++)
				{
					stPROFILE *pProfile = &pInfo->arrProfile[profileIdx];
					if (pProfile->iCall < 4)
						continue;

					__int64 totalTime = pProfile->iTotalTime;
					totalTime -= pProfile->iMaxTime[0];
					totalTime -= pProfile->iMaxTime[1];
					totalTime -= pProfile->iMinTime[0];
					totalTime -= pProfile->iMinTime[1];

					if (pProfile->iCall - 4 > 0)
					{
						__int64 average = totalTime / (pProfile->iCall - 4);

						fprintf_s(fp, "%-15d | %-100s | %-15lld | %-15lld | %-15lld | %-15lld | %-15lld |\n"
							, pInfo->threadId
							, pProfile->szName
							, totalTime
							, average
							, pProfile->iMinTime[0]
							, pProfile->iMaxTime[0]
							, pProfile->iCall - 4);
					}
				}

				fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
			}

			fclose(fp);
		}

		void ResetProfile()
		{
			for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
			{
				stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

				// pInfo�� Reset Flag�� ����
				pInfo->resetFlag = TRUE;
			}
		}
		void Clear()
		{
			for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
			{
				stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];
				pInfo->Clear();
				delete pInfo;
			}

			m_lCurrentInfosIdx = 0;
		}

		// TlsIndex �Ҵ�
		inline LONG AllocThreadInfo()
		{
			LONG idx = InterlockedIncrement(&m_lCurrentInfosIdx);
			if (m_iMaxThreadCount + 1 <= idx)
			{
				__debugbreak();
			}
			m_arrProfileInfos[idx] = new stTlsProfileInfo;
			m_arrProfileInfos[idx]->Init();

			return idx;
		}

	public:
		DWORD						m_dwInfosTlsIdx; // Tls �ε���
		static constexpr int		m_iMaxThreadCount = 1000; // �ִ� ������ ����
		LONG						m_lCurrentInfosIdx = 0; // ��ȿ �ε��� - Tls Ư�� �� 0���� ��� �Ұ�
		stTlsProfileInfo *m_arrProfileInfos[m_iMaxThreadCount]; // Tls Profile ����
		LARGE_INTEGER m_lFreq;

		LONG m_lResetFlag = FALSE;

		std::map<std::wstring, stPROFILE> m_mapProfiles;
	};

	inline CProfileManager *g_ProfileMgr = nullptr;

	// RAII �������� Profile ���� ���
	class CProfile
	{
	public:
		CProfile(int index, const char *funcName, const char *tagName)
		{
			UINT64 idx = (UINT64)TlsGetValue(g_ProfileMgr->m_dwInfosTlsIdx);

			stPROFILE *pProfile = &g_ProfileMgr->m_arrProfileInfos[idx]->arrProfile[index];
			if (pProfile->isUsed == false)
			{
				strcat_s(pProfile->szName, 300, funcName);
				if (tagName != nullptr)
				{
					strcat_s(pProfile->szName, 300 - strlen(funcName), " ");
					strcat_s(pProfile->szName, 300 - strlen(funcName) - strlen(" "), tagName);
				}

				pProfile->isUsed = true;
			}

			// ���� reset Flag �� �����ִٸ�
			// Reset�� �����ϰ� �ٽ� ����
			// ������ ��ϰ� ������ ���� �����忡�� ����ǹǷ�
			// Interlocked �迭�� ����ȭ�� �ʿ����
			// �׸��� �뷫���� ���� �������κ����� �׽�Ʈ ����� �ʿ��ϹǷ� ��Ȯ�� Ÿ�ֿ̹� ������ �ʿ��� �͵� �ƴ�
			if (g_ProfileMgr->m_arrProfileInfos[idx]->resetFlag)
			{
				g_ProfileMgr->m_arrProfileInfos[idx]->Reset();
				g_ProfileMgr->m_arrProfileInfos[idx]->resetFlag = FALSE;
			}

			m_stProfile = pProfile;
			m_iStartTime = std::chrono::high_resolution_clock::now();

			// std::chrono::steady_clock::time_point
			// QueryPerformanceCounter(&m_iStartTime);
		}
		~CProfile()
		{
			// LARGE_INTEGER endTime;
			// QueryPerformanceCounter(&endTime);

			std::chrono::steady_clock::time_point endTime = std::chrono::high_resolution_clock::now();
			// __int64 runtime = endTime.QuadPart - m_iStartTime.QuadPart;
			std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_iStartTime);
			__int64 runtime = ns.count();
			m_stProfile->iCall++;
			m_stProfile->iTotalTime += runtime;

			// MAX ����
			if (m_stProfile->iMaxTime[1] < runtime)
			{
				m_stProfile->iMaxTime[1] = runtime;
				if (m_stProfile->iMaxTime[0] < m_stProfile->iMaxTime[1])
				{
					__int64 temp = m_stProfile->iMaxTime[0];
					m_stProfile->iMaxTime[0] = m_stProfile->iMaxTime[1];
					m_stProfile->iMaxTime[1] = temp;
				}
			}

			// MIN ����
			if (m_stProfile->iMinTime[1] > runtime)
			{
				m_stProfile->iMinTime[1] = runtime;
				if (m_stProfile->iMinTime[0] > m_stProfile->iMinTime[1])
				{
					__int64 temp = m_stProfile->iMinTime[0];
					m_stProfile->iMinTime[0] = m_stProfile->iMinTime[1];
					m_stProfile->iMinTime[1] = temp;
				}
			}
		}

		stPROFILE *m_stProfile;
		std::chrono::steady_clock::time_point m_iStartTime;
		// LARGE_INTEGER	m_iStartTime;
	};
}