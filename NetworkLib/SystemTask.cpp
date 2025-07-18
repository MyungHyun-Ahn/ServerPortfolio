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
		m_timeMs = 1000; // 1��
		m_nextExecuteTime = timeGetTime(); // ���� �ð�
	}

	void KeyBoardTimerTask::execute(int delayFrame) noexcept
	{
		// ���� ����
		if (GetAsyncKeyState(VK_F1))
			NetworkLib::Core::Net::Server::g_NetServer->Stop();

		// �������Ϸ� ����
		if (GetAsyncKeyState(VK_F2))
			MHLib::utils::g_ProfileMgr->DataOutToFile();
	}
}