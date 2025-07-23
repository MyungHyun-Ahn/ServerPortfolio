#pragma once

namespace NetworkLib::Core::Utils
{
	using namespace MHLib::utils;

	class CCoreLibInit
	{
	public:

		// ���� ��ü�� ���� Lib���� ����ϴ� �̱��� ��ü �ʱ�ȭ
		CCoreLibInit()
		{
			// ������ �������� ����
			timeBeginPeriod(1);
			g_Logger = CLogger::GetInstance(); // SetMainDirectory�� SetLogLevel�� ��ӹ��� Cpp ������Ʈ���� ����
			g_ProfileMgr = CProfileManager::GetInstance();
			
		}


		~CCoreLibInit()
		{
			timeEndPeriod(1);
		}
	};

	inline static CCoreLibInit coreInit;
}