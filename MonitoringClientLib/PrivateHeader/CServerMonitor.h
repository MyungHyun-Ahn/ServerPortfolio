#pragma once


namespace MonitoringClientLib::Monitoring
{
	using MHLib::utils::g_Logger;
	using MHLib::utils::LOG_LEVEL;

	class CServerMonitor : public CMonitor, public MHLib::utils::Singleton<CServerMonitor>
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

