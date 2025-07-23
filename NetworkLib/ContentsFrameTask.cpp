#include "pch.h"
#include "CNetServer.h"
#include "CBaseContents.h"
#include "ContentsFrameTask.h"

namespace NetworkLib::Task
{
	void ContentsFrameTask::SetEvent(NetworkLib::Contents::CBaseContents *pBaseContent, int frame) noexcept
	{
		m_pBaseContents = pBaseContent;
		// TimerEvent의 종료를 제어하기 위함
		m_pBaseContents->m_pTimerEvent = this;
		if (frame == 0)
			m_timeMs = 0;
		else
			m_timeMs = 1000 / frame;
		m_nextExecuteTime = timeGetTime();
	}

	void ContentsFrameTask::execute(int delayFrame) noexcept
	{
		InterlockedIncrement(&m_pBaseContents->m_FPS);

		m_pBaseContents->ProcessMoveJob();
		m_pBaseContents->ProcessLeaveJob();
		m_pBaseContents->ProcessRecvMsg(delayFrame);
	}
}