#pragma once

namespace MHLib::utils
{
	class CMonitor
	{
	protected:
		CMonitor() = default;
		virtual ~CMonitor() = default;

	public:
		virtual void MonitoringFunc() final
		{
			Update();
			PrintConsole();
			Send();
			Reset();
		}

	protected:
		virtual void Update() = 0;
		virtual void PrintConsole() = 0;
		virtual void Send() = 0; // 모니터링 서버로 모니터링 데이터를 전송
		virtual void Reset() = 0; // 값을 재설정 해야 하는 경우 ex) TPS
	};
}