#pragma once


namespace MonitoringClientLib::Monitoring
{
	class CServerMonitor : public MHLib::utils::CMonitor, public MHLib::utils::Singleton<CServerMonitor>
	{
	private:
		CServerMonitor() = default;
		virtual ~CServerMonitor() = default;

	private:
		void Update() override;
		void PrintConsole() override;
		void Send() override;
		void Reset() override;

	private:
		LONG64 m_llLoopCount = 0;
		LONG64 m_llAcceptTotal = 0;
		LONG64 m_llSendTotal = 0;
		LONG64 m_llRecvTotal = 0;
	};
}

