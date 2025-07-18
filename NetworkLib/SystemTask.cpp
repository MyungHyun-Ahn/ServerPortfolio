#include "pch.h"
#include "SystemTask.h"
#include "CoreUtils.h"
#include "NetSetting.h"
#include "CContentsThread.h"
#include "PrivateHeader/MonitoringTarget.h"
#include "CNetServer.h"


namespace NetworkLib::Task
{
	void MonitorTimerTask::SetEvent() noexcept
	{
	}

	void MonitorTimerTask::execute(int delayFrame) noexcept
	{
	}

	void KeyBoardTimerTask::SetEvent() noexcept
	{
		m_timeMs = 1000; // 1초
		m_nextExecuteTime = timeGetTime(); // 현재 시각
	}

	void KeyBoardTimerTask::execute(int delayFrame) noexcept
	{
		// 서버 종료
		if (GetAsyncKeyState(VK_F1))
			NetworkLib::Core::Net::Server::g_NetServer->Stop();

		// 프로파일러 저장
		if (GetAsyncKeyState(VK_F2))
			MHLib::utils::g_ProfileMgr->DataOutToFile();
	}
}