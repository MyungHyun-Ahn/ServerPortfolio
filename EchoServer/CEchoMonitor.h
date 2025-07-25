#pragma once

namespace EchoServer::Monitor
{
	class CEchoMonitor : public MHLib::utils::CMonitor, public MHLib::utils::Singleton<CEchoMonitor>
	{
	private:
		CEchoMonitor() = default;
		virtual ~CEchoMonitor() = default;

	private:
		void Update() override;
		void PrintConsole() override;
		void Send() override;
		void Reset() override;

	private:
		LONG m_lAuthFPS = 0;
		LONG m_lEchoFPS = 0;
	};
}

