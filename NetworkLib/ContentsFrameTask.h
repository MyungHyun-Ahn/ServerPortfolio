#pragma once
namespace NetworkLib::Task
{
	class ContentsFrameTask : public TimerTask
	{
		void SetEvent(NetworkLib::Contents::CBaseContents *pBaseContent, int frame) noexcept;
		void execute(int delayFrame) noexcept override;

		NetworkLib::Contents::CBaseContents *m_pBaseContents;
	};
}