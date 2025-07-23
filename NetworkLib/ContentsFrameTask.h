#pragma once
namespace NetworkLib::Task
{
	using NetworkLib::Contents::CBaseContents;
	class ContentsFrameTask : public TimerTask
	{
		void SetEvent(CBaseContents *pBaseContent, int frame) noexcept;
		void execute(int delayFrame) noexcept override;

		CBaseContents *m_pBaseContents;
	};
}