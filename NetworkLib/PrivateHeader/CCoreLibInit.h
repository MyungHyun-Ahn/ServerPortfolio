#pragma once

namespace NetworkLib::Core::Utils
{
	class CCoreLibInit
	{
	public:

		// ���� ��ü�� ���� Lib���� ����ϴ� �̱��� ��ü �ʱ�ȭ
		CCoreLibInit()
		{
			// ������ �������� ����
			timeBeginPeriod(1);
			MHLib::utils::g_Logger = MHLib::utils::CLogger::GetInstance(); // SetMainDirectory�� SetLogLevel�� ��ӹ��� Cpp ������Ʈ���� ����
			MHLib::utils::g_ProfileMgr = MHLib::utils::CProfileManager::GetInstance();
			MHLib::utils::g_MonitoringMgr = MHLib::utils::CMonitoringManager::GetInstance(); // SetConsoleSize�� Cpp ������Ʈ����
		}


		~CCoreLibInit()
		{
			timeEndPeriod(1);
		}
	};

	inline static CCoreLibInit coreInit;
}