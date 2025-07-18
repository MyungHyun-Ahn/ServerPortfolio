#pragma once

#include <windows.h>

#pragma comment(lib, "DbgHelp.Lib")
#include <crtdbg.h>
#include <DbgHelp.h>

#define CRASH_DUMP_ON

namespace MHLib::debug
{
	class CCrashDump
	{
	public:
		static void Init()
		{
			InitializeSRWLock(&m_srwLock);

			_invalid_parameter_handler oldHandler, newHandler;
			newHandler = myInvalidParameterHandler;

			oldHandler = _set_invalid_parameter_handler(newHandler);	// CRT �Լ��� nullptr ����
			_CrtSetReportMode(_CRT_WARN, 0);							// CRT ���� �޽��� ǥ�� �ߴ�. �ٷ� ������ ���⵵��
			_CrtSetReportMode(_CRT_ASSERT, 0);							// CRT ���� �޽��� ǥ�� �ߴ�. �ٷ� ������ ���⵵��
			_CrtSetReportMode(_CRT_ERROR, 0);							// CRT ���� �޽��� ǥ�� �ߴ�. �ٷ� ������ ���⵵��

			_CrtSetReportHook(_custom_Report_hook);

			// pure virtual function called ���� �ڵ鷯�� ����� ���� �Լ��� ��ȸ
			_set_purecall_handler(myPurecallHandler);

			SetHandlerDump();
		}

		// debugbreak�� ���α׷� ũ���ø� ����
		static void Crash()
		{
			__debugbreak();
		}

		// ���� ���� �߻��� ������ ������Ű�� �Լ�
		// ī����
		//    ��Ƽ ������ ȯ�濡���� �� �����忡�� ���� �߻��� �������� �ٸ� �����尡 ���ܸ� �߻���Ŵ
		//    �� ��� ù��° ���� �ڵ鷯�� ������ �����ϴ� �������� �ٸ� ���� �ڵ鷯�� ���� ���ų� ����� ������ ������ ���� �� ����
		//    �׷��� ���� ������ �̸����� �ѹ����Ͽ� ������ ����
		static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
		{
			SYSTEMTIME nowTime;

			WCHAR fileName[MAX_PATH];

			AcquireSRWLockExclusive(&m_srwLock);
			// Dump�� ���� �ʿ䰡 ����
			if (m_bDumpFlag)
			{
				ReleaseSRWLockExclusive(&m_srwLock);
				return EXCEPTION_EXECUTE_HANDLER;
			}

			// ���� �ð��� �˾ƿ�
			GetLocalTime(&nowTime);
			wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d.%02d.dmp", nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond);

			MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::ERR, L"\n\n\n Crash Error!!!");
			MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::ERR, L"Saving dump file...");

			HANDLE hDumpFile = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hDumpFile != INVALID_HANDLE_VALUE)
			{
				_MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInformation{ 0, };
				miniDumpExceptionInformation.ThreadId = ::GetCurrentThreadId();
				miniDumpExceptionInformation.ExceptionPointers = pExceptionPointer;
				miniDumpExceptionInformation.ClientPointers = TRUE;

				MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, &miniDumpExceptionInformation, NULL, NULL);
				CloseHandle(hDumpFile);
				MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::ERR, L"CrashDump saved completed");
				// 1�� �������� �������� �ٸ� �� �ȳ����
				m_bDumpFlag = true;
			}
			else
			{
				// ���� ����µ� �����ߴٸ� - �ٽ� �õ��� �غ�
				// �ٵ� �Ƹ� �� ������ ����
				MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::ERR, L"CrashDump failed");
			}

			ReleaseSRWLockExclusive(&m_srwLock);
			return EXCEPTION_EXECUTE_HANDLER;
		}

		static void SetHandlerDump()
		{
			SetUnhandledExceptionFilter(MyExceptionFilter);
		}

		static void myInvalidParameterHandler(const wchar_t *expression, const wchar_t *function, const wchar_t *file, unsigned int line, uintptr_t pReserved)
		{
			Crash();
		}

		static int _custom_Report_hook(int ireposttype, char *message, int *returnvalue)
		{
			Crash();
			return true;
		}

		static void myPurecallHandler(void)
		{
			Crash();
		}

	public:
		inline static long m_lDumpCount = 0;
		inline static SRWLOCK m_srwLock;
		inline static bool m_bDumpFlag = false;
	};

#ifdef CRASH_DUMP_ON
	inline CCrashDump crashDump;
#endif
}