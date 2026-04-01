#include "Pch.h"

#include "Diagnostics/FCrashDump.h"

#include <DbgHelp.h>
#include <crtdbg.h>

#pragma comment(lib, "DbgHelp.lib")

namespace
{
	using TInvalidParameterHandler = _invalid_parameter_handler;
	using TPurecallHandler = _purecall_handler;
	using TCrtReportHook = _CRT_REPORT_HOOK;

	struct SCrashDumpState
	{
		GameServer::Foundation::SCrashDumpConfig config{};
		SRWLOCK lock{};
		LONG initialized = 0;
		LONG dumpWritten = 0;
		LONG dumpSequence = 0;
		LPTOP_LEVEL_EXCEPTION_FILTER previousExceptionFilter = nullptr;
		TInvalidParameterHandler previousInvalidParameterHandler = nullptr;
		TPurecallHandler previousPurecallHandler = nullptr;
		TCrtReportHook previousReportHook = nullptr;
	};

	SCrashDumpState g_state{};

	std::string BuildTimestamp()
	{
		const auto now = std::chrono::system_clock::now();
		const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

		std::tm localTime{};
		localtime_s(&localTime, &nowTime);

		std::ostringstream oss;
		oss << std::put_time(&localTime, "%Y%m%d_%H%M%S");
		return oss.str();
	}

	void Log(GameServer::Foundation::ELogLevel logLevel, const std::string& message)
	{
		if (g_state.config.logger != nullptr)
		{
			g_state.config.logger->Log(logLevel, "CrashDump", message);
			return;
		}

		OutputDebugStringA((message + "\n").c_str());
	}

	MINIDUMP_TYPE ToMiniDumpType(GameServer::Foundation::ECrashDumpType dumpType)
	{
		switch (dumpType)
		{
		case GameServer::Foundation::ECrashDumpType::WithFullMemory:
			return MiniDumpWithFullMemory;
		case GameServer::Foundation::ECrashDumpType::Normal:
		default:
			return MiniDumpNormal;
		}
	}

	std::wstring BuildDumpPath()
	{
		const std::filesystem::path directoryPath = std::filesystem::path(g_state.config.outputDirectory);
		std::error_code errorCode;
		std::filesystem::create_directories(directoryPath, errorCode);

		const LONG dumpSequence = InterlockedIncrement(&g_state.dumpSequence);
		const DWORD processId = GetCurrentProcessId();

		std::ostringstream fileName;
		fileName << "CrashDump_"
			<< BuildTimestamp()
			<< "_pid" << processId
			<< "_seq" << dumpSequence
			<< ".dmp";

		return (directoryPath / fileName.str()).wstring();
	}

	bool ShouldSkipDump()
	{
		if (!g_state.config.allowOnlySingleDump)
		{
			return false;
		}

		return InterlockedCompareExchange(&g_state.dumpWritten, 1, 0) != 0;
	}

	bool WriteDumpInternal(EXCEPTION_POINTERS* exceptionPointers)
	{
		if (!g_state.config.enabled)
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "Crash dump request ignored because module is disabled.");
			return false;
		}

		if (ShouldSkipDump())
		{
			Log(GameServer::Foundation::ELogLevel::Warn, "Crash dump request ignored because a dump was already written.");
			return false;
		}

		const std::wstring dumpPath = BuildDumpPath();
		HANDLE dumpFileHandle = CreateFileW(
			dumpPath.c_str(),
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);

		if (dumpFileHandle == INVALID_HANDLE_VALUE)
		{
			std::ostringstream oss;
			oss << "CreateFileW failed while creating dump. error=" << GetLastError();
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());

			if (g_state.config.allowOnlySingleDump)
			{
				InterlockedExchange(&g_state.dumpWritten, 0);
			}

			return false;
		}

		MINIDUMP_EXCEPTION_INFORMATION exceptionInformation{};
		MINIDUMP_EXCEPTION_INFORMATION* exceptionInformationPtr = nullptr;
		if (exceptionPointers != nullptr)
		{
			exceptionInformation.ThreadId = GetCurrentThreadId();
			exceptionInformation.ExceptionPointers = exceptionPointers;
			exceptionInformation.ClientPointers = TRUE;
			exceptionInformationPtr = &exceptionInformation;
		}

		const BOOL dumpWritten = MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			dumpFileHandle,
			ToMiniDumpType(g_state.config.dumpType),
			exceptionInformationPtr,
			nullptr,
			nullptr);

		CloseHandle(dumpFileHandle);

		if (dumpWritten == FALSE)
		{
			std::ostringstream oss;
			oss << "MiniDumpWriteDump failed. error=" << GetLastError();
			Log(GameServer::Foundation::ELogLevel::Error, oss.str());

			if (g_state.config.allowOnlySingleDump)
			{
				InterlockedExchange(&g_state.dumpWritten, 0);
			}

			return false;
		}

		std::ostringstream oss;
		oss << "Crash dump written: " << std::filesystem::path(dumpPath).string();
		Log(GameServer::Foundation::ELogLevel::Error, oss.str());
		return true;
	}

	LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* exceptionPointers)
	{
		AcquireSRWLockExclusive(&g_state.lock);
		WriteDumpInternal(exceptionPointers);
		ReleaseSRWLockExclusive(&g_state.lock);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	void HandleInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
	{
		RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, 0, nullptr);
	}

	int __cdecl HandleCrtReport(int, char*, int*)
	{
		RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, 0, nullptr);
		return TRUE;
	}

	void HandlePurecall()
	{
		RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, 0, nullptr);
	}
}

namespace GameServer::Foundation
{
	bool FCrashDump::Initialize(const SCrashDumpConfig& config)
	{
		AcquireSRWLockExclusive(&g_state.lock);

		if (g_state.initialized != 0)
		{
			ReleaseSRWLockExclusive(&g_state.lock);
			Log(ELogLevel::Warn, "Crash dump initialize request ignored because it is already initialized.");
			return false;
		}

		g_state.config = config;
		g_state.dumpWritten = 0;
		g_state.dumpSequence = 0;
		g_state.previousExceptionFilter = SetUnhandledExceptionFilter(&HandleUnhandledException);

		if (g_state.config.installInvalidParameterHandler)
		{
			g_state.previousInvalidParameterHandler = _set_invalid_parameter_handler(&HandleInvalidParameter);
		}

		if (g_state.config.installPurecallHandler)
		{
			g_state.previousPurecallHandler = _set_purecall_handler(&HandlePurecall);
		}

		if (g_state.config.installCrtReportHook)
		{
			g_state.previousReportHook = _CrtSetReportHook(&HandleCrtReport);
			_CrtSetReportMode(_CRT_WARN, 0);
			_CrtSetReportMode(_CRT_ASSERT, 0);
			_CrtSetReportMode(_CRT_ERROR, 0);
		}

		g_state.initialized = 1;
		ReleaseSRWLockExclusive(&g_state.lock);

		Log(ELogLevel::Info, "Crash dump module initialized.");
		return true;
	}

	void FCrashDump::Shutdown()
	{
		AcquireSRWLockExclusive(&g_state.lock);

		if (g_state.initialized == 0)
		{
			ReleaseSRWLockExclusive(&g_state.lock);
			return;
		}

		SetUnhandledExceptionFilter(g_state.previousExceptionFilter);

		if (g_state.config.installInvalidParameterHandler)
		{
			_set_invalid_parameter_handler(g_state.previousInvalidParameterHandler);
		}

		if (g_state.config.installPurecallHandler)
		{
			_set_purecall_handler(g_state.previousPurecallHandler);
		}

		if (g_state.config.installCrtReportHook)
		{
			_CrtSetReportHook(g_state.previousReportHook);
		}

		g_state.initialized = 0;
		ReleaseSRWLockExclusive(&g_state.lock);
		Log(ELogLevel::Info, "Crash dump module shut down.");
	}

	bool FCrashDump::IsInitialized()
	{
		return g_state.initialized != 0;
	}

	bool FCrashDump::WriteManualDumpForDiagnostics()
	{
		if (!IsInitialized())
		{
			Log(ELogLevel::Warn, "Manual dump request ignored because crash dump module is not initialized.");
			return false;
		}

		AcquireSRWLockExclusive(&g_state.lock);
		const bool result = WriteDumpInternal(nullptr);
		ReleaseSRWLockExclusive(&g_state.lock);
		return result;
	}
}
