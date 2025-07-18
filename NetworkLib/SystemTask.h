#pragma once
namespace NetworkLib::Task
{
	struct MonitorTimerTask : public TimerTask
	{
		void SetEvent() noexcept;
		void execute(int delayFrame) noexcept override;
	};

	struct KeyBoardTimerTask : public TimerTask
	{
		void SetEvent() noexcept;
		void execute(int delayFrame) noexcept override;
	};
}