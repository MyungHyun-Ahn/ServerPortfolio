#pragma once

namespace NetworkLib::Core::Utils
{
	using namespace MHLib::utils;

	class CCoreLibInit
	{
	public:

		// 전역 객체를 통한 Lib에서 사용하는 싱글톤 객체 초기화
		CCoreLibInit()
		{
			// 정밀한 프레임을 위해
			timeBeginPeriod(1);
			g_Logger = CLogger::GetInstance(); // SetMainDirectory와 SetLogLevel은 상속받은 Cpp 프로젝트에서 설정
			g_ProfileMgr = CProfileManager::GetInstance();
			
		}


		~CCoreLibInit()
		{
			timeEndPeriod(1);
		}
	};

	inline static CCoreLibInit coreInit;
}