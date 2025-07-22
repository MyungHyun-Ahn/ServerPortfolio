#pragma once

namespace NetworkLib::Core::Monitoring
{
	struct ServerMonitoringTargets
	{
		LONG sessionCount = 0;
		LONG acceptTPS = 0;
		LONG sendTPS = 0;
		LONG recvTPS = 0;
		LONG maxSendCount = 0;
		LONG sendPendingCount = 0;
		LONG sendCallTPS = 0; // ÃÊ´ç WSASend ÄÝ È½¼ö
	};

	struct ContentsThreadMonitoringTargets
	{
		LONG delegateWorkCount = 0;
		LONG workStealingCount = 0;
	};
}