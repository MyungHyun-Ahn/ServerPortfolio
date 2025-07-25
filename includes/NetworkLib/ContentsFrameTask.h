#pragma once

namespace NetworkLib::Task
{
	class ContentsFrameTask : public TimerTask
	{
	public:
		void SetEvent(NetworkLib::Contents::CBaseContents *pBaseContent, int frame) noexcept;
		void execute(int delayFrame) noexcept override;

	private:
		NetworkLib::Contents::CBaseContents *m_pBaseContents;
	};
}