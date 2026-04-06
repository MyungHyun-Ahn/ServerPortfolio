#include "Pch.h"

#include "ContentsRuntime/Routing/FContentRuntime.h"

#include "ContentsRuntime/Core/IContent.h"
#include "ContentsRuntime/Threading/FContentThread.h"
#include "Servers/IServer.h"

#include <algorithm>
#include <limits>

namespace ContentsRuntime::Routing
{
	namespace
	{
		inline constexpr std::uint64_t kSessionSlotMask = 0xFFFFFFFFULL;

		bool ShouldTraceSession(const Core::SContentRuntimeConfig& config, const std::uint64_t sessionId)
		{
			if (!config.enableTraceLogging || !config.traceLogger)
			{
				return false;
			}

			if (config.tracedSessionId == nullptr)
			{
				return true;
			}

			return config.tracedSessionId->load(std::memory_order_relaxed) == sessionId;
		}

		void TraceRuntime(const Core::SContentRuntimeConfig& config, const std::uint64_t sessionId, const std::string& message)
		{
			if (!ShouldTraceSession(config, sessionId))
			{
				return;
			}

			config.traceLogger(message);
		}

		template <typename TValue>
		void UpdateMaxAtomic(std::atomic<TValue>& target, TValue candidate)
		{
			TValue current = target.load(std::memory_order_relaxed);
			while (current < candidate &&
				!target.compare_exchange_weak(current, candidate, std::memory_order_relaxed))
			{
			}
		}

		std::uint64_t ToNanoseconds(const std::chrono::steady_clock::duration duration)
		{
			return static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
		}

		std::uint32_t DecodeSessionSlotIndex(const std::uint64_t sessionId) noexcept
		{
			return static_cast<std::uint32_t>(sessionId & kSessionSlotMask);
		}

		[[noreturn]] void FailFastRuntime(const char* message) noexcept
		{
			if (message != nullptr)
			{
				::OutputDebugStringA(message);
				::OutputDebugStringA("\n");
			}

			::TerminateProcess(::GetCurrentProcess(), 0xE001);
			std::abort();
		}

		void RunRaceInjection(const Core::SContentRuntimeConfig& config, std::atomic<std::uint64_t>& counter) noexcept
		{
			if (!config.enableRaceInjection || config.raceInjectionPeriod == 0 || config.raceInjectionMode == Core::ERaceInjectionMode::None)
			{
				return;
			}

			const std::uint64_t current = counter.fetch_add(1, std::memory_order_relaxed) + 1;
			if ((current % config.raceInjectionPeriod) != 0)
			{
				return;
			}

			switch (config.raceInjectionMode)
			{
			case Core::ERaceInjectionMode::SwitchToThread:
				::SwitchToThread();
				break;
			case Core::ERaceInjectionMode::Sleep0:
				::Sleep(0);
				break;
			case Core::ERaceInjectionMode::Yield:
				std::this_thread::yield();
				break;
			default:
				break;
			}
		}

	}

	struct SContentSlot
	{
		Core::FContentId contentId = Core::kInvalidContentId;
		Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
		std::unique_ptr<Core::IContent> content;
		std::uint32_t workerIndex = 0;
		Threading::FContentThread* worker = nullptr;
	};

	struct SSessionRoute
	{
		enum class EMoveState : std::uint8_t
		{
			Idle,
			Pending
		};

		std::uint64_t sessionId = 0;
		std::uint64_t routeGeneration = 0;
		Core::FContentId contentId = Core::kInvalidContentId;
		Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
		std::uint32_t workerIndex = 0;
		Threading::FContentThread* worker = nullptr;
		EMoveState moveState = EMoveState::Idle;
		Core::FContentId pendingTargetContentId = Core::kInvalidContentId;
		Core::FContentInstanceId pendingTargetContentInstanceId = Core::kInvalidContentInstanceId;
		std::uint32_t pendingTargetWorkerIndex = 0;
		Threading::FContentThread* pendingTargetWorker = nullptr;
		std::uint64_t pendingTargetRouteGeneration = 0;
		std::deque<Core::FOwnedPacketEnvelope> pendingPackets;
	};

	struct FContentRuntime::SImpl
	{
		NetworkLib::IServer* server = nullptr;
		mutable std::shared_mutex lock;
		std::unordered_map<Core::FContentInstanceId, SContentSlot> contentSlots;
		std::unordered_map<Core::FContentId, Core::FContentInstanceId> defaultInstanceIdsByContentId;
		std::vector<std::unique_ptr<Threading::FContentThread>> workers;
		std::vector<SSessionRoute> sessionRoutes;
		Core::SContentRuntimeConfig config{};
		std::atomic<std::uint64_t> activeSessionCount = 0;
		std::atomic<std::uint64_t> enterSessionCallCount = 0;
		std::atomic<std::uint64_t> leaveSessionCallCount = 0;
		std::atomic<std::uint64_t> enqueuePacketCallCount = 0;
		std::atomic<std::uint64_t> moveSessionCount = 0;
		std::atomic<std::uint64_t> enqueueFailureCount = 0;
		std::atomic<std::uint64_t> enterSessionLockWaitNs = 0;
		std::atomic<std::uint64_t> leaveSessionLockWaitNs = 0;
		std::atomic<std::uint64_t> enqueuePacketLockWaitNs = 0;
		std::atomic<std::uint64_t> moveSessionLockWaitNs = 0;
		std::atomic<std::uint64_t> maxEnterSessionLockWaitNs = 0;
		std::atomic<std::uint64_t> maxLeaveSessionLockWaitNs = 0;
		std::atomic<std::uint64_t> maxEnqueuePacketLockWaitNs = 0;
		std::atomic<std::uint64_t> maxMoveSessionLockWaitNs = 0;
		std::atomic<std::uint64_t> raceInjectionCounter = 0;
	};

	FContentRuntime::FContentRuntime()
		: m_impl(std::make_unique<SImpl>())
	{
	}

	FContentRuntime::~FContentRuntime()
	{
		Stop();
	}

	bool FContentRuntime::RegisterContent(std::unique_ptr<Core::IContent> content)
	{
		if (content == nullptr)
		{
			return false;
		}

		std::unique_lock<std::shared_mutex> lock(m_impl->lock);
		if (m_impl->server != nullptr)
		{
			return false;
		}

		const Core::FContentId contentId = content->GetContentId();
		const Core::FContentInstanceId contentInstanceId = content->GetContentInstanceId();
		if (contentId == Core::kInvalidContentId ||
			contentInstanceId == Core::kInvalidContentInstanceId ||
			m_impl->contentSlots.contains(contentInstanceId))
		{
			return false;
		}

		SContentSlot slot{};
		slot.contentId = contentId;
		slot.contentInstanceId = contentInstanceId;
		slot.content = std::move(content);
		m_impl->contentSlots.emplace(contentInstanceId, std::move(slot));
		m_impl->defaultInstanceIdsByContentId.try_emplace(contentId, contentInstanceId);
		return true;
	}

	void FContentRuntime::SetConfig(const Core::SContentRuntimeConfig& config)
	{
		std::unique_lock<std::shared_mutex> lock(m_impl->lock);
		if (m_impl->server != nullptr)
		{
			return;
		}

		m_impl->config = config;
	}

	void FContentRuntime::Start(NetworkLib::IServer& server)
	{
		std::unique_lock<std::shared_mutex> lock(m_impl->lock);
		if (m_impl->server != nullptr)
		{
			return;
		}

		m_impl->server = &server;
		const std::uint32_t sessionCapacity =
			std::max<std::uint32_t>(server.GetStatsSnapshot().sessionPoolCapacity, 1024u);
		m_impl->sessionRoutes.clear();
		m_impl->sessionRoutes.resize(sessionCapacity);
		m_impl->activeSessionCount.store(0, std::memory_order_relaxed);

		const std::uint32_t workerCount = std::max<std::uint32_t>(
			1u,
			std::min<std::uint32_t>(
				std::max<std::uint32_t>(1u, m_impl->config.workerThreadCount),
				static_cast<std::uint32_t>(std::max<std::size_t>(1, m_impl->contentSlots.size()))));
		m_impl->workers.clear();
		m_impl->workers.reserve(workerCount);
		for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
		{
			m_impl->workers.push_back(std::make_unique<Threading::FContentThread>(*this, m_impl->config, workerIndex));
		}

		std::vector<SContentSlot*> orderedSlots;
		orderedSlots.reserve(m_impl->contentSlots.size());
		for (auto& [contentInstanceId, slot] : m_impl->contentSlots)
		{
			(void)contentInstanceId;
			orderedSlots.push_back(&slot);
		}

		std::sort(
			orderedSlots.begin(),
			orderedSlots.end(),
			[](const SContentSlot* lhs, const SContentSlot* rhs)
			{
				return lhs->contentInstanceId < rhs->contentInstanceId;
			});

		std::uint32_t nextWorkerIndex = 0;
		for (SContentSlot* slot : orderedSlots)
		{
			Threading::FContentThread& worker = *m_impl->workers[nextWorkerIndex];
			slot->workerIndex = nextWorkerIndex;
			slot->worker = &worker;
			worker.RegisterContent(*slot->content);
			nextWorkerIndex = (nextWorkerIndex + 1) % workerCount;
		}

		for (auto& worker : m_impl->workers)
		{
			worker->Start();
		}
	}

	void FContentRuntime::Stop()
	{
		std::vector<std::unique_ptr<Threading::FContentThread>> workersToStop;
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			for (auto& [contentInstanceId, slot] : m_impl->contentSlots)
			{
				(void)contentInstanceId;
				slot.worker = nullptr;
				slot.workerIndex = 0;
			}
			workersToStop = std::move(m_impl->workers);
			for (SSessionRoute& route : m_impl->sessionRoutes)
			{
				route = {};
			}
			m_impl->activeSessionCount.store(0, std::memory_order_relaxed);
			m_impl->server = nullptr;
		}

		for (auto& worker : workersToStop)
		{
			worker->Stop();
		}
	}

	Core::SContentRuntimeStats FContentRuntime::GetStatsSnapshot()
	{
		Core::SContentRuntimeStats stats{};
		std::unordered_map<Core::FContentInstanceId, std::uint64_t> sessionCounts;
		struct SSnapshotSlot
		{
			Core::FContentId contentId = Core::kInvalidContentId;
			Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
			Threading::FContentThread* worker = nullptr;
		};
		std::vector<SSnapshotSlot> snapshotSlots;

		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			stats.registeredContentCount = static_cast<std::uint64_t>(m_impl->contentSlots.size());
			stats.activeSessionCount = m_impl->activeSessionCount.load(std::memory_order_relaxed);
			for (const SSessionRoute& route : m_impl->sessionRoutes)
			{
				if (route.sessionId == 0 || route.contentInstanceId == Core::kInvalidContentInstanceId)
				{
					continue;
				}

				++sessionCounts[route.contentInstanceId];
			}

			for (auto& [contentInstanceId, slot] : m_impl->contentSlots)
			{
				snapshotSlots.push_back({ slot.contentId, contentInstanceId, slot.worker });
			}
		}

		stats.contents.reserve(snapshotSlots.size());
		for (const SSnapshotSlot& snapshotSlot : snapshotSlots)
		{
			Core::SContentRuntimeContentStats contentStats{};
			contentStats.contentId = snapshotSlot.contentId;
			contentStats.contentInstanceId = snapshotSlot.contentInstanceId;
			contentStats.activeSessionCount = sessionCounts[snapshotSlot.contentInstanceId];
			if (snapshotSlot.worker != nullptr)
			{
				contentStats.threadStats = snapshotSlot.worker->GetStatsSnapshot(snapshotSlot.contentInstanceId);
			}
			else
			{
				contentStats.threadStats.contentId = snapshotSlot.contentId;
				contentStats.threadStats.contentInstanceId = snapshotSlot.contentInstanceId;
			}
			stats.contents.push_back(std::move(contentStats));
		}

		stats.enterSessionCallCount = m_impl->enterSessionCallCount.load(std::memory_order_relaxed);
		stats.leaveSessionCallCount = m_impl->leaveSessionCallCount.load(std::memory_order_relaxed);
		stats.enqueuePacketCallCount = m_impl->enqueuePacketCallCount.load(std::memory_order_relaxed);
		stats.moveSessionCount = m_impl->moveSessionCount.load(std::memory_order_relaxed);
		stats.enqueueFailureCount = m_impl->enqueueFailureCount.load(std::memory_order_relaxed);
		stats.enterSessionLockWaitNs = m_impl->enterSessionLockWaitNs.load(std::memory_order_relaxed);
		stats.leaveSessionLockWaitNs = m_impl->leaveSessionLockWaitNs.load(std::memory_order_relaxed);
		stats.enqueuePacketLockWaitNs = m_impl->enqueuePacketLockWaitNs.load(std::memory_order_relaxed);
		stats.moveSessionLockWaitNs = m_impl->moveSessionLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnterSessionLockWaitNs = m_impl->maxEnterSessionLockWaitNs.load(std::memory_order_relaxed);
		stats.maxLeaveSessionLockWaitNs = m_impl->maxLeaveSessionLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueuePacketLockWaitNs = m_impl->maxEnqueuePacketLockWaitNs.load(std::memory_order_relaxed);
		stats.maxMoveSessionLockWaitNs = m_impl->maxMoveSessionLockWaitNs.load(std::memory_order_relaxed);
		return stats;
	}

	bool FContentRuntime::EnterSession(std::uint64_t sessionId, Core::FContentId initialContentId)
	{
		Core::FContentInstanceId initialContentInstanceId = Core::kInvalidContentInstanceId;
		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
			const auto defaultIt = m_impl->defaultInstanceIdsByContentId.find(initialContentId);
			if (defaultIt == m_impl->defaultInstanceIdsByContentId.end())
			{
				return false;
			}

			initialContentInstanceId = defaultIt->second;
		}

		return EnterSessionToInstance(sessionId, initialContentInstanceId);
	}

	bool FContentRuntime::EnterSessionToInstance(std::uint64_t sessionId, Core::FContentInstanceId initialContentInstanceId)
	{
		m_impl->enterSessionCallCount.fetch_add(1, std::memory_order_relaxed);
		Threading::FContentThread* targetWorker = nullptr;
		std::uint64_t targetRouteGeneration = 0;
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
			m_impl->enterSessionLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxEnterSessionLockWaitNs, lockWaitNs);
			if (slotIndex >= m_impl->sessionRoutes.size())
			{
				return false;
			}

			auto contentIt = m_impl->contentSlots.find(initialContentInstanceId);
			if (contentIt == m_impl->contentSlots.end() || contentIt->second.worker == nullptr)
			{
				return false;
			}

			SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId == 0)
			{
				m_impl->activeSessionCount.fetch_add(1, std::memory_order_relaxed);
				route.routeGeneration = 0;
			}
			targetRouteGeneration = route.routeGeneration + 1;
			route.sessionId = sessionId;
			route.routeGeneration = targetRouteGeneration;
			route.contentId = contentIt->second.contentId;
			route.contentInstanceId = initialContentInstanceId;
			route.workerIndex = contentIt->second.workerIndex;
			targetWorker = contentIt->second.worker;
			route.worker = targetWorker;
		}

		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		if (!targetWorker->EnqueueEnter({ sessionId, targetRouteGeneration, initialContentInstanceId }))
		{
			return false;
		}
		return true;
	}

	void FContentRuntime::LeaveSession(std::uint64_t sessionId)
	{
		m_impl->leaveSessionCallCount.fetch_add(1, std::memory_order_relaxed);
		Threading::FContentThread* targetWorker = nullptr;
		std::uint64_t routeGeneration = 0;
		Core::FContentInstanceId currentContentInstanceId = Core::kInvalidContentInstanceId;
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
			m_impl->leaveSessionLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxLeaveSessionLockWaitNs, lockWaitNs);
			if (slotIndex >= m_impl->sessionRoutes.size())
			{
				return;
			}

			SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId != sessionId)
			{
				return;
			}
			currentContentInstanceId = route.contentInstanceId;
			routeGeneration = route.routeGeneration;
			targetWorker = route.worker;
			route = {};
			m_impl->activeSessionCount.fetch_sub(1, std::memory_order_relaxed);

			if (targetWorker == nullptr)
			{
				auto contentIt = m_impl->contentSlots.find(currentContentInstanceId);
				if (contentIt != m_impl->contentSlots.end())
				{
					targetWorker = contentIt->second.worker;
				}
			}
		}

		if (targetWorker != nullptr)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			targetWorker->EnqueueLeave({ sessionId, routeGeneration, currentContentInstanceId });
		}
	}

	bool FContentRuntime::EnqueuePacket(std::uint64_t sessionId, std::uint16_t opcode, const char* payload, std::int32_t payloadLength)
	{
		m_impl->enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		Threading::FContentThread* targetWorker = nullptr;
		std::uint64_t routeGeneration = 0;
		Core::FContentId targetContentId = Core::kInvalidContentId;
		Core::FContentInstanceId targetContentInstanceId = Core::kInvalidContentInstanceId;
		bool bufferedForPendingMove = false;
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
			m_impl->enqueuePacketLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxEnqueuePacketLockWaitNs, lockWaitNs);
			if (slotIndex >= m_impl->sessionRoutes.size())
			{
				m_impl->enqueueFailureCount.fetch_add(1, std::memory_order_relaxed);
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime enqueue failed: session slot out of range.");
				}
				return false;
			}

			SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId != sessionId || route.worker == nullptr)
			{
				m_impl->enqueueFailureCount.fetch_add(1, std::memory_order_relaxed);
				if (m_impl->config.enableTraceLogging)
				{
					std::ostringstream oss;
					oss << "runtime enqueue rejected. sessionId=" << sessionId
						<< " opcode=" << opcode
						<< " slotIndex=" << slotIndex
						<< " routeSessionId=" << route.sessionId
						<< " hasWorker=" << (route.worker != nullptr ? 1 : 0);
					TraceRuntime(m_impl->config, sessionId, oss.str());
				}
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime enqueue failed: route mismatch or null worker.");
				}
				return false;
			}

			targetWorker = route.worker;
			routeGeneration = route.routeGeneration;
			targetContentId = route.contentId;
			targetContentInstanceId = route.contentInstanceId;

			if (route.moveState == SSessionRoute::EMoveState::Pending)
			{
				Core::FOwnedPacketEnvelope pendingPacket{};
				pendingPacket.sessionId = sessionId;
				pendingPacket.opcode = opcode;
				if (payload != nullptr && payloadLength > 0)
				{
					pendingPacket.payload.assign(payload, payload + payloadLength);
				}

				targetContentId = route.pendingTargetContentId;
				targetContentInstanceId = route.pendingTargetContentInstanceId;
				routeGeneration = route.pendingTargetRouteGeneration;
				route.pendingPackets.push_back(std::move(pendingPacket));
				bufferedForPendingMove = true;
			}
		}

		if (m_impl->config.enableTraceLogging)
		{
			std::ostringstream oss;
			oss << "runtime enqueue accepted. sessionId=" << sessionId
				<< " opcode=" << opcode
				<< " routeGeneration=" << routeGeneration
				<< " contentId=" << targetContentId
				<< " contentInstanceId=" << targetContentInstanceId
				<< " payloadBytes=" << payloadLength
				<< " bufferedForPendingMove=" << (bufferedForPendingMove ? 1 : 0);
			TraceRuntime(m_impl->config, sessionId, oss.str());
		}

		if (!bufferedForPendingMove)
		{
			Core::FOwnedPacketEnvelope packet{};
			packet.sessionId = sessionId;
			packet.routeGeneration = routeGeneration;
			packet.contentInstanceId = targetContentInstanceId;
			packet.opcode = opcode;
			if (payload != nullptr && payloadLength > 0)
			{
				packet.payload.assign(payload, payload + payloadLength);
			}

			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			if (!targetWorker->EnqueuePacket(packet))
			{
				m_impl->enqueueFailureCount.fetch_add(1, std::memory_order_relaxed);
				if (m_impl->config.enableTraceLogging)
				{
					std::ostringstream oss;
					oss << "runtime enqueue failed after direct post miss. sessionId=" << sessionId
						<< " opcode=" << opcode
						<< " routeGeneration=" << routeGeneration
						<< " contentInstanceId=" << targetContentInstanceId;
					TraceRuntime(m_impl->config, sessionId, oss.str());
				}
				return false;
			}
			if (m_impl->config.enableTraceLogging)
			{
				std::ostringstream oss;
				oss << "runtime enqueue posted. sessionId=" << sessionId
					<< " opcode=" << opcode
					<< " routeGeneration=" << routeGeneration
					<< " contentId=" << targetContentId
					<< " contentInstanceId=" << targetContentInstanceId;
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}
		}
		return true;
	}

	bool FContentRuntime::SendPacket(
		std::uint64_t sessionId,
		NetworkLib::Packet::Serialization::FOutgoingContentPacket&& packet)
	{
		NetworkLib::IServer* server = nullptr;
		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
			server = m_impl->server;
		}

		return server != nullptr && server->SendPacket(sessionId, std::move(packet));
	}

	bool FContentRuntime::MoveSession(std::uint64_t sessionId, Core::FContentId targetContentId)
	{
		return MoveSessionWithCompletion(sessionId, targetContentId, {});
	}

	bool FContentRuntime::MoveSessionWithCompletion(
		std::uint64_t sessionId,
		Core::FContentId targetContentId,
		Core::FTransitionCompletionCallback onCompleted)
	{
		Core::FContentInstanceId targetContentInstanceId = Core::kInvalidContentInstanceId;
		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
			const auto defaultIt = m_impl->defaultInstanceIdsByContentId.find(targetContentId);
			if (defaultIt == m_impl->defaultInstanceIdsByContentId.end())
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: default target content instance missing.");
				}
				return false;
			}

			targetContentInstanceId = defaultIt->second;
		}

		return MoveSessionToInstanceWithCompletion(sessionId, targetContentInstanceId, std::move(onCompleted));
	}

	bool FContentRuntime::MoveSessionToInstance(std::uint64_t sessionId, Core::FContentInstanceId targetContentInstanceId)
	{
		return MoveSessionToInstanceWithCompletion(sessionId, targetContentInstanceId, {});
	}

	bool FContentRuntime::MoveSessionToInstanceWithCompletion(
		std::uint64_t sessionId,
		Core::FContentInstanceId targetContentInstanceId,
		Core::FTransitionCompletionCallback onCompleted)
	{
		Threading::FContentThread* sourceWorker = nullptr;
		Threading::FContentThread* targetWorker = nullptr;
		Core::FContentId sourceContentId = Core::kInvalidContentId;
		Core::FContentId targetContentId = Core::kInvalidContentId;
		Core::FContentInstanceId sourceContentInstanceId = Core::kInvalidContentInstanceId;
		std::uint32_t sourceWorkerIndex = 0;
		std::uint32_t targetWorkerIndex = 0;
		std::uint64_t sourceRouteGeneration = 0;
		std::uint64_t targetRouteGeneration = 0;
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		{
			std::ostringstream oss;
			oss << "move begin. sessionId=" << sessionId
				<< " requestedTargetContentInstanceId=" << targetContentInstanceId;
			TraceRuntime(m_impl->config, sessionId, oss.str());
		}
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
			m_impl->moveSessionLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxMoveSessionLockWaitNs, lockWaitNs);
			if (slotIndex >= m_impl->sessionRoutes.size())
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: session slot out of range.");
				}
				return false;
			}

			auto targetIt = m_impl->contentSlots.find(targetContentInstanceId);
			if (targetIt == m_impl->contentSlots.end() || targetIt->second.worker == nullptr)
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: target content worker missing.");
				}
				return false;
			}

			targetWorker = targetIt->second.worker;
			targetContentId = targetIt->second.contentId;

			SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId != sessionId)
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: route mismatch.");
				}
				return false;
			}

			if (route.moveState != SSessionRoute::EMoveState::Idle)
			{
				TraceRuntime(m_impl->config, sessionId, "move rejected because another move is already pending.");
				return false;
			}

			sourceContentInstanceId = route.contentInstanceId;
			sourceContentId = route.contentId;
			sourceWorkerIndex = route.workerIndex;
			if (route.worker != nullptr)
			{
				sourceWorker = route.worker;
			}
			else
			{
				const auto sourceIt = m_impl->contentSlots.find(route.contentInstanceId);
				if (sourceIt != m_impl->contentSlots.end() && sourceIt->second.worker != nullptr)
				{
					sourceWorker = sourceIt->second.worker;
				}
			}

			sourceRouteGeneration = route.routeGeneration;
			targetRouteGeneration = sourceRouteGeneration + 1;
			targetWorkerIndex = targetIt->second.workerIndex;
			{
				std::ostringstream oss;
				oss << "move route plan. sessionId=" << sessionId
					<< " sourceContentInstanceId=" << sourceContentInstanceId
					<< " sourceContentId=" << sourceContentId
					<< " sourceRouteGeneration=" << sourceRouteGeneration
					<< " sourceWorkerIndex=" << sourceWorkerIndex
					<< " targetContentInstanceId=" << targetContentInstanceId
					<< " targetContentId=" << targetContentId
					<< " targetWorkerIndex=" << targetWorkerIndex
					<< " targetRouteGeneration=" << targetRouteGeneration;
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}

			{
				std::ostringstream oss;
				oss << "move route deferred. sessionId=" << sessionId
					<< " currentRouteContentInstanceId=" << route.contentInstanceId
					<< " routeGeneration=" << route.routeGeneration
					<< " routeWorkerIndex=" << route.workerIndex;
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}

			route.moveState = SSessionRoute::EMoveState::Pending;
			route.pendingTargetContentId = targetContentId;
			route.pendingTargetContentInstanceId = targetContentInstanceId;
			route.pendingTargetWorkerIndex = targetWorkerIndex;
			route.pendingTargetWorker = targetWorker;
			route.pendingTargetRouteGeneration = targetRouteGeneration;
			route.pendingPackets.clear();
		}

		m_impl->moveSessionCount.fetch_add(1, std::memory_order_relaxed);

		auto replayPendingMovePackets =
			[this, sessionId](std::vector<Core::FOwnedPacketEnvelope>& packets)
		{
			for (Core::FOwnedPacketEnvelope& packet : packets)
			{
				const char* payloadData = packet.payload.empty() ? nullptr : packet.payload.data();
				const std::int32_t payloadSize = static_cast<std::int32_t>(packet.payload.size());
				if (!this->EnqueuePacket(sessionId, packet.opcode, payloadData, payloadSize))
				{
					if (m_impl->config.enableTraceLogging)
					{
						std::ostringstream oss;
						oss << "move buffered packet replay failed. sessionId=" << sessionId
							<< " opcode=" << packet.opcode
							<< " payloadBytes=" << packet.payload.size();
						TraceRuntime(m_impl->config, sessionId, oss.str());
					}
				}
			}
		};

		auto cancelPendingMoveAndReplayToCurrentRoute =
			[this, sessionId, slotIndex, replayPendingMovePackets](const char* reason)
		{
			std::vector<Core::FOwnedPacketEnvelope> pendingPackets;
			{
				std::unique_lock<std::shared_mutex> lock(m_impl->lock);
				if (slotIndex >= m_impl->sessionRoutes.size())
				{
					return;
				}

				SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
				if (route.sessionId != sessionId || route.moveState != SSessionRoute::EMoveState::Pending)
				{
					return;
				}

				pendingPackets.reserve(route.pendingPackets.size());
				while (!route.pendingPackets.empty())
				{
					pendingPackets.push_back(std::move(route.pendingPackets.front()));
					route.pendingPackets.pop_front();
				}

				route.moveState = SSessionRoute::EMoveState::Idle;
				route.pendingTargetContentId = Core::kInvalidContentId;
				route.pendingTargetContentInstanceId = Core::kInvalidContentInstanceId;
				route.pendingTargetWorkerIndex = 0;
				route.pendingTargetWorker = nullptr;
				route.pendingTargetRouteGeneration = 0;
			}

			if (m_impl->config.enableTraceLogging)
			{
				std::ostringstream oss;
				oss << "move pending cancelled. sessionId=" << sessionId
					<< " reason=" << (reason != nullptr ? reason : "unknown")
					<< " replayPacketCount=" << pendingPackets.size();
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}

			replayPendingMovePackets(pendingPackets);
		};

		Core::FTransitionCompletionCallback wrappedOnCompleted =
			[this,
			sessionId,
			slotIndex,
			sourceContentId,
			sourceContentInstanceId,
			sourceWorkerIndex,
			sourceWorker,
			sourceRouteGeneration,
			targetContentId,
			targetContentInstanceId,
			targetWorkerIndex,
			targetWorker,
			targetRouteGeneration,
			replayPendingMovePackets,
			callback = std::move(onCompleted)]() mutable
		{
			bool routeCommitted = false;
			std::vector<Core::FOwnedPacketEnvelope> pendingPackets;
			{
				std::unique_lock<std::shared_mutex> lock(m_impl->lock);
				if (slotIndex < m_impl->sessionRoutes.size())
				{
					SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
					if (route.sessionId == sessionId &&
						route.moveState == SSessionRoute::EMoveState::Pending &&
						route.contentId == sourceContentId &&
						route.contentInstanceId == sourceContentInstanceId &&
						route.routeGeneration == sourceRouteGeneration &&
						route.workerIndex == sourceWorkerIndex &&
						route.worker == sourceWorker)
					{
						route.contentId = targetContentId;
						route.contentInstanceId = targetContentInstanceId;
						route.routeGeneration = targetRouteGeneration;
						route.workerIndex = targetWorkerIndex;
						route.worker = targetWorker;
						route.moveState = SSessionRoute::EMoveState::Idle;
						route.pendingTargetContentId = Core::kInvalidContentId;
						route.pendingTargetContentInstanceId = Core::kInvalidContentInstanceId;
						route.pendingTargetWorkerIndex = 0;
						route.pendingTargetWorker = nullptr;
						route.pendingTargetRouteGeneration = 0;
						pendingPackets.reserve(route.pendingPackets.size());
						while (!route.pendingPackets.empty())
						{
							pendingPackets.push_back(std::move(route.pendingPackets.front()));
							route.pendingPackets.pop_front();
						}
						routeCommitted = true;
					}
				}
			}

			if (m_impl->config.enableTraceLogging)
			{
				std::ostringstream oss;
				oss << "move route commit. sessionId=" << sessionId
					<< " committed=" << (routeCommitted ? 1 : 0)
					<< " sourceContentInstanceId=" << sourceContentInstanceId
					<< " sourceRouteGeneration=" << sourceRouteGeneration
					<< " targetContentInstanceId=" << targetContentInstanceId
					<< " targetRouteGeneration=" << targetRouteGeneration
					<< " targetWorkerIndex=" << targetWorkerIndex
					<< " replayPacketCount=" << pendingPackets.size();
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}

			if (callback)
			{
				callback();
			}

			replayPendingMovePackets(pendingPackets);
		};

		const bool useSameWorkerMoveFastPath =
			sourceWorker != nullptr &&
			targetWorker != nullptr &&
			sourceWorker == targetWorker;

		if (useSameWorkerMoveFastPath)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			const bool transitionPosted = sourceWorker->EnqueueMoveTransition(
				{ sessionId, sourceRouteGeneration, sourceContentInstanceId },
				{ sessionId, targetRouteGeneration, targetContentInstanceId, nullptr, std::move(wrappedOnCompleted) });
			{
				std::ostringstream oss;
				oss << "move same-worker fast-path enqueue. sessionId=" << sessionId
					<< " workerIndex=" << sourceWorker->GetWorkerIndex()
					<< " sourceContentInstanceId=" << sourceContentInstanceId
					<< " sourceRouteGeneration=" << sourceRouteGeneration
					<< " targetContentInstanceId=" << targetContentInstanceId
					<< " targetRouteGeneration=" << targetRouteGeneration
					<< " posted=" << (transitionPosted ? 1 : 0);
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}
			if (!transitionPosted)
			{
				cancelPendingMoveAndReplayToCurrentRoute("same-worker-fast-path-post-failed");
				TraceRuntime(m_impl->config, sessionId, "move same-worker fast-path failed before route commit.");
				return false;
			}

			TraceRuntime(m_impl->config, sessionId, "move completed successfully.");
			return true;
		}

		if (sourceWorker != nullptr)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			const bool leavePosted =
				sourceWorker->EnqueueLeave({ sessionId, sourceRouteGeneration, sourceContentInstanceId });
			{
				std::ostringstream oss;
				oss << "move source leave enqueue. sessionId=" << sessionId
					<< " sourceWorkerIndex=" << sourceWorker->GetWorkerIndex()
					<< " sourceContentInstanceId=" << sourceContentInstanceId
					<< " sourceRouteGeneration=" << sourceRouteGeneration
					<< " posted=" << (leavePosted ? 1 : 0);
				TraceRuntime(m_impl->config, sessionId, oss.str());
			}
			if (!leavePosted)
			{
				cancelPendingMoveAndReplayToCurrentRoute("source-leave-post-failed");
				TraceRuntime(m_impl->config, sessionId, "move source leave failed before route commit.");
				return false;
			}
		}

		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		Core::SContentLifecycleEvent targetEnterEvent{
			sessionId,
			targetRouteGeneration,
			targetContentInstanceId,
			nullptr,
			std::move(wrappedOnCompleted) };
		const bool enterPosted = targetWorker->EnqueueEnter(targetEnterEvent);
		{
			std::ostringstream oss;
			oss << "move target enter enqueue. sessionId=" << sessionId
				<< " targetWorkerIndex=" << targetWorker->GetWorkerIndex()
				<< " targetContentInstanceId=" << targetContentInstanceId
				<< " targetRouteGeneration=" << targetRouteGeneration
				<< " hasCompletionCallback=" << (targetEnterEvent.completionCallback ? 1 : 0)
				<< " posted=" << (enterPosted ? 1 : 0);
			TraceRuntime(m_impl->config, sessionId, oss.str());
		}
		if (!enterPosted)
		{
			cancelPendingMoveAndReplayToCurrentRoute("target-enter-post-failed");
			TraceRuntime(m_impl->config, sessionId, "move target enter failed before route commit.");
			return false;
		}
		TraceRuntime(m_impl->config, sessionId, "move completed successfully.");
		return true;
	}

	bool FContentRuntime::DisconnectSession(std::uint64_t sessionId)
	{
		NetworkLib::IServer* server = nullptr;
		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
			server = m_impl->server;
		}

		return server != nullptr && server->Disconnect(sessionId);
	}

	bool FContentRuntime::IsSessionAlive(std::uint64_t sessionId) const
	{
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		std::shared_lock<std::shared_mutex> lock(m_impl->lock);
		if (slotIndex >= m_impl->sessionRoutes.size())
		{
			return false;
		}

		return m_impl->sessionRoutes[slotIndex].sessionId == sessionId;
	}

	bool FContentRuntime::HasContentInstance(Core::FContentInstanceId contentInstanceId) const
	{
		std::shared_lock<std::shared_mutex> lock(m_impl->lock);
		return m_impl->contentSlots.contains(contentInstanceId);
	}

	std::optional<Core::FContentId> FContentRuntime::GetCurrentContentId(std::uint64_t sessionId) const
	{
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		std::shared_lock<std::shared_mutex> lock(m_impl->lock);
		if (slotIndex >= m_impl->sessionRoutes.size())
		{
			return std::nullopt;
		}

		const SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
		if (route.sessionId != sessionId || route.contentId == Core::kInvalidContentId)
		{
			return std::nullopt;
		}

		return route.contentId;
	}

	std::optional<Core::FContentInstanceId> FContentRuntime::GetCurrentContentInstanceId(std::uint64_t sessionId) const
	{
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		std::shared_lock<std::shared_mutex> lock(m_impl->lock);
		if (slotIndex >= m_impl->sessionRoutes.size())
		{
			return std::nullopt;
		}

		const SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
		if (route.sessionId != sessionId || route.contentInstanceId == Core::kInvalidContentInstanceId)
		{
			return std::nullopt;
		}

		return route.contentInstanceId;
	}

}
