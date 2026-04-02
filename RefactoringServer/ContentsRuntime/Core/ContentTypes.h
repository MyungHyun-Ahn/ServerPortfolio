#pragma once

namespace ContentsRuntime::Core
{
	using FContentId = std::uint16_t;
	inline constexpr FContentId kInvalidContentId = 0;

	enum class ERaceInjectionMode : std::uint8_t
	{
		None = 0,
		SwitchToThread = 1,
		Sleep0 = 2,
		Yield = 3
	};

	struct SContentRuntimeConfig
	{
		bool enableRaceInjection = false;
		bool failFastOnRuntimeError = false;
		std::uint32_t raceInjectionPeriod = 0;
		ERaceInjectionMode raceInjectionMode = ERaceInjectionMode::SwitchToThread;
	};

	struct FOwnedPacketEnvelope
	{
		std::uint64_t sessionId = 0;
		std::uint16_t opcode = 0;
		std::vector<char> payload;
	};

	struct SContentThreadStats
	{
		FContentId contentId = kInvalidContentId;
		bool running = false;
		std::uint64_t enqueueEnterCallCount = 0;
		std::uint64_t enqueueLeaveCallCount = 0;
		std::uint64_t enqueuePacketCallCount = 0;
		std::uint64_t enterCount = 0;
		std::uint64_t leaveCount = 0;
		std::uint64_t packetCount = 0;
		std::uint64_t frameCount = 0;
		std::uint64_t enterQueueDepth = 0;
		std::uint64_t leaveQueueDepth = 0;
		std::uint64_t packetQueueDepth = 0;
		std::uint64_t maxEnterQueueDepth = 0;
		std::uint64_t maxLeaveQueueDepth = 0;
		std::uint64_t maxPacketQueueDepth = 0;
		std::uint64_t enqueueEnterLockWaitNs = 0;
		std::uint64_t enqueueLeaveLockWaitNs = 0;
		std::uint64_t enqueuePacketLockWaitNs = 0;
		std::uint64_t maxEnqueueEnterLockWaitNs = 0;
		std::uint64_t maxEnqueueLeaveLockWaitNs = 0;
		std::uint64_t maxEnqueuePacketLockWaitNs = 0;
		int lastDelayFrame = 0;
		int maxDelayFrame = 0;
	};

	struct SContentRuntimeContentStats
	{
		FContentId contentId = kInvalidContentId;
		std::uint64_t activeSessionCount = 0;
		SContentThreadStats threadStats;
	};

	struct SContentRuntimeStats
	{
		std::uint64_t registeredContentCount = 0;
		std::uint64_t activeSessionCount = 0;
		std::uint64_t enterSessionCallCount = 0;
		std::uint64_t leaveSessionCallCount = 0;
		std::uint64_t enqueuePacketCallCount = 0;
		std::uint64_t moveSessionCount = 0;
		std::uint64_t enqueueFailureCount = 0;
		std::uint64_t enterSessionLockWaitNs = 0;
		std::uint64_t leaveSessionLockWaitNs = 0;
		std::uint64_t enqueuePacketLockWaitNs = 0;
		std::uint64_t moveSessionLockWaitNs = 0;
		std::uint64_t maxEnterSessionLockWaitNs = 0;
		std::uint64_t maxLeaveSessionLockWaitNs = 0;
		std::uint64_t maxEnqueuePacketLockWaitNs = 0;
		std::uint64_t maxMoveSessionLockWaitNs = 0;
		std::vector<SContentRuntimeContentStats> contents;
	};
}
