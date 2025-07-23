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
		virtual void Send() = 0; // ����͸� ������ ����͸� �����͸� ����
		virtual void Reset() = 0; // ���� �缳�� �ؾ� �ϴ� ��� ex) TPS
	};
}