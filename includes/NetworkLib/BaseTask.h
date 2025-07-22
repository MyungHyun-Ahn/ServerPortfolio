#pragma once

namespace NetworkLib::Task
{
	struct BaseTask
	{
	public:
		BaseTask(bool isTimer = false) : m_isTimerTask(isTimer) {}
		virtual void execute(int delayFrame) = 0;

	public:
		bool m_isTimerTask;
	};

	struct TimerTask : public BaseTask
	{
	public:
		TimerTask() : BaseTask(true), m_isRunning(true) {}

	public:
		bool m_isRunning;
		DWORD m_timeMs;
		DWORD m_nextExecuteTime;
	};

	struct TimerTaskComparator
	{
		bool operator()(const TimerTask *t1, const TimerTask *t2) const noexcept
		{
			return t1->m_nextExecuteTime > t2->m_nextExecuteTime; // 실행시간 빠른 것을 우선
		}
	};
}