#pragma once

namespace NetworkLib::Core::Utils
{
	class CCoreLibInit
	{
	public:

		// 전역 객체를 통한 Lib에서 사용하는 싱글톤 객체 초기화
		CCoreLibInit()
		{
			// 정밀한 프레임을 위해
			timeBeginPeriod(1);
			MHLib::utils::g_Logger = MHLib::utils::CLogger::GetInstance(); // SetMainDirectory와 SetLogLevel은 상속받은 Cpp 프로젝트에서 설정
			MHLib::utils::g_ProfileMgr = MHLib::utils::CProfileManager::GetInstance();
			MHLib::utils::g_MonitoringMgr = MHLib::utils::CMonitoringManager::GetInstance(); // SetConsoleSize는 Cpp 프로젝트에서
		}


		~CCoreLibInit()
		{
			timeEndPeriod(1);
		}
	};

	inline static CCoreLibInit coreInit;
}