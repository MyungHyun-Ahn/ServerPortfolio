#include "Pch.h"

#include "ContentsRuntime/Threading/FContentThread.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "ContentsRuntime/Core/IContent.h"

namespace ContentsRuntime::Threading
{
	namespace
	{
		enum class EQueuedWorkKind : std::uint8_t
		{
			Enter,
			Leave,
			Packet
		};

		template <typename TValue>
		void UpdateMaxAtomic(std::atomic<TValue>& target, const TValue candidate)
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

		struct SQueuedWorkItem
		{
			EQueuedWorkKind kind = EQueuedWorkKind::Packet;
			Core::SContentLifecycleEvent lifecycleEvent{};
			Core::FOwnedPacketEnvelope packet{};

			void Reset() noexcept
			{
				kind = EQueuedWorkKind::Packet;
				lifecycleEvent = {};
				packet.sessionId = 0;
				packet.routeGeneration = 0;
				packet.contentInstanceId = Core::kInvalidContentInstanceId;
				packet.opcode = 0;
				packet.payload.clear();
			}

			USE_TLS_POOL_WITH_INIT(SQueuedWorkItem, s_pool, Reset)
		};

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

		void TraceThread(
			const Core::SContentRuntimeConfig& config,
			const std::uint64_t sessionId,
			const std::string& message)
		{
			if (!ShouldTraceSession(config, sessionId))
			{
				return;
			}

			config.traceLogger(message);
		}
	}

	struct FContentThread::SImpl
	{
		struct SPerContentState
		{
			Core::IContent* content = nullptr;
			Core::FContentId contentId = Core::kInvalidContentId;
			Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
			std::chrono::milliseconds frameDuration{ 33 };
			std::chrono::steady_clock::time_point nextFrameTime{};
			std::atomic<std::uint64_t> enqueueEnterCallCount = 0;
			std::atomic<std::uint64_t> enqueueLeaveCallCount = 0;
			std::atomic<std::uint64_t> enqueuePacketCallCount = 0;
			std::atomic<std::uint64_t> enterCount = 0;
			std::atomic<std::uint64_t> leaveCount = 0;
			std::atomic<std::uint64_t> packetCount = 0;
			std::atomic<std::uint64_t> frameCount = 0;
			std::atomic<std::uint64_t> enterQueueDepth = 0;
			std::atomic<std::uint64_t> leaveQueueDepth = 0;
			std::atomic<std::uint64_t> packetQueueDepth = 0;
			std::atomic<std::uint64_t> maxEnterQueueDepth = 0;
			std::atomic<std::uint64_t> maxLeaveQueueDepth = 0;
			std::atomic<std::uint64_t> maxPacketQueueDepth = 0;
			std::atomic<std::uint64_t> enqueueEnterLockWaitNs = 0;
			std::atomic<std::uint64_t> enqueueLeaveLockWaitNs = 0;
			std::atomic<std::uint64_t> enqueuePacketLockWaitNs = 0;
			std::atomic<std::uint64_t> maxEnqueueEnterLockWaitNs = 0;
			std::atomic<std::uint64_t> maxEnqueueLeaveLockWaitNs = 0;
			std::atomic<std::uint64_t> maxEnqueuePacketLockWaitNs = 0;
			std::atomic<int> lastDelayFrame = 0;
			std::atomic<int> maxDelayFrame = 0;
		};

		Bridge::IContentBridge* bridge = nullptr;
		Core::SContentRuntimeConfig config{};
		std::uint32_t workerIndex = 0;
		std::thread workerThread;
		std::mutex lock;
		std::condition_variable wakeCondition;
		NetworkLib::Containers::FLockFreeQueue<SQueuedWorkItem*> workQueueLockFree;
		std::unordered_map<Core::FContentInstanceId, SPerContentState> contents;
		std::atomic<std::uint64_t> pendingWorkCount = 0;
		std::atomic<std::uint64_t> raceInjectionCounter = 0;
		bool running = false;
	};

	FContentThread::FContentThread(
		Bridge::IContentBridge& bridge,
		const Core::SContentRuntimeConfig& config,
		const std::uint32_t workerIndex)
		: m_impl(std::make_unique<SImpl>())
	{
		m_impl->bridge = &bridge;
		m_impl->config = config;
		m_impl->workerIndex = workerIndex;
	}

	FContentThread::~FContentThread()
	{
		Stop();
	}

	bool FContentThread::RegisterContent(Core::IContent& content)
	{
		std::lock_guard<std::mutex> lock(m_impl->lock);
		if (m_impl->running)
		{
			return false;
		}

		const Core::FContentInstanceId contentInstanceId = content.GetContentInstanceId();
		if (content.GetContentId() == Core::kInvalidContentId ||
			contentInstanceId == Core::kInvalidContentInstanceId ||
			m_impl->contents.contains(contentInstanceId))
		{
			return false;
		}

		auto [stateIt, inserted] = m_impl->contents.try_emplace(contentInstanceId);
		if (!inserted)
		{
			return false;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.content = &content;
		state.contentId = content.GetContentId();
		state.contentInstanceId = contentInstanceId;
		const std::uint32_t targetFps = std::max<std::uint32_t>(1u, content.GetTargetFps());
		state.frameDuration =
			std::chrono::milliseconds(std::max<std::int64_t>(1, 1000 / static_cast<std::int64_t>(targetFps)));
		state.nextFrameTime = std::chrono::steady_clock::now() + state.frameDuration;
		return true;
	}

	void FContentThread::Start()
	{
		if (m_impl->running)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			const auto now = std::chrono::steady_clock::now();
			for (auto& [contentInstanceId, state] : m_impl->contents)
			{
				(void)contentInstanceId;
				state.nextFrameTime = now + state.frameDuration;
			}
			m_impl->running = true;
		}

		m_impl->workerThread = std::thread([this]()
		{
			SImpl& impl = *m_impl;

			while (true)
			{
				{
					std::unique_lock<std::mutex> lock(impl.lock);
					auto nextWakeTime = std::chrono::steady_clock::time_point::max();
					for (const auto& [contentInstanceId, state] : impl.contents)
					{
						(void)contentInstanceId;
						if (state.nextFrameTime < nextWakeTime)
						{
							nextWakeTime = state.nextFrameTime;
						}
					}

					if (nextWakeTime == std::chrono::steady_clock::time_point::max())
					{
						impl.wakeCondition.wait(lock, [&impl]()
						{
							return !impl.running ||
								impl.pendingWorkCount.load(std::memory_order_relaxed) > 0;
						});
					}
					else
					{
						impl.wakeCondition.wait_until(
							lock,
							nextWakeTime,
							[&impl]()
							{
								return !impl.running ||
									impl.pendingWorkCount.load(std::memory_order_relaxed) > 0;
							});
					}

					if (!impl.running)
					{
						break;
					}
				}

				std::vector<SQueuedWorkItem*> workItems;
				SQueuedWorkItem* queuedWorkItem = nullptr;
				RunRaceInjection(impl.config, impl.raceInjectionCounter);
				while (impl.workQueueLockFree.Dequeue(queuedWorkItem))
				{
					if (queuedWorkItem != nullptr)
					{
						workItems.push_back(queuedWorkItem);
					}
				}

				if (!workItems.empty())
				{
					impl.pendingWorkCount.fetch_sub(static_cast<std::uint64_t>(workItems.size()), std::memory_order_relaxed);
				}

				for (SQueuedWorkItem* workItem : workItems)
				{
					if (workItem == nullptr)
					{
						continue;
					}

					Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
					switch (workItem->kind)
					{
					case EQueuedWorkKind::Enter:
						contentInstanceId = workItem->lifecycleEvent.contentInstanceId;
						break;
					case EQueuedWorkKind::Leave:
						contentInstanceId = workItem->lifecycleEvent.contentInstanceId;
						break;
					case EQueuedWorkKind::Packet:
						contentInstanceId = workItem->packet.contentInstanceId;
						break;
					default:
						break;
					}

					auto stateIt = impl.contents.find(contentInstanceId);
					if (stateIt == impl.contents.end() || stateIt->second.content == nullptr)
					{
						workItem->Reset();
						SQueuedWorkItem::Free(workItem);
						continue;
					}

					SImpl::SPerContentState& state = stateIt->second;
					switch (workItem->kind)
					{
					case EQueuedWorkKind::Enter:
						state.content->OnEnter(
							workItem->lifecycleEvent.sessionId,
							workItem->lifecycleEvent.routeGeneration,
							*impl.bridge);
						if (workItem->lifecycleEvent.completionFlag != nullptr)
						{
							workItem->lifecycleEvent.completionFlag->store(true, std::memory_order_release);
						}
						if (workItem->lifecycleEvent.completionCallback)
						{
							workItem->lifecycleEvent.completionCallback();
						}
						state.enterCount.fetch_add(1, std::memory_order_relaxed);
						state.enterQueueDepth.fetch_sub(1, std::memory_order_relaxed);
						break;

					case EQueuedWorkKind::Leave:
						state.content->OnLeave(
							workItem->lifecycleEvent.sessionId,
							workItem->lifecycleEvent.routeGeneration,
							*impl.bridge);
						if (workItem->lifecycleEvent.completionFlag != nullptr)
						{
							workItem->lifecycleEvent.completionFlag->store(true, std::memory_order_release);
						}
						if (workItem->lifecycleEvent.completionCallback)
						{
							workItem->lifecycleEvent.completionCallback();
						}
						state.leaveCount.fetch_add(1, std::memory_order_relaxed);
						state.leaveQueueDepth.fetch_sub(1, std::memory_order_relaxed);
						break;

					case EQueuedWorkKind::Packet:
						if (impl.config.enableTraceLogging)
						{
							std::ostringstream oss;
							oss << "worker dequeue packet. workerIndex=" << impl.workerIndex
								<< " sessionId=" << workItem->packet.sessionId
								<< " opcode=" << workItem->packet.opcode
								<< " routeGeneration=" << workItem->packet.routeGeneration
								<< " contentId=" << state.contentId
								<< " contentInstanceId=" << state.contentInstanceId
								<< " payloadBytes=" << workItem->packet.payload.size();
							TraceThread(impl.config, workItem->packet.sessionId, oss.str());
						}

						state.content->OnPacket(
							workItem->packet.sessionId,
							workItem->packet.routeGeneration,
							workItem->packet.opcode,
							std::span<const char>(workItem->packet.payload.data(), workItem->packet.payload.size()),
							*impl.bridge);
						state.packetCount.fetch_add(1, std::memory_order_relaxed);
						state.packetQueueDepth.fetch_sub(1, std::memory_order_relaxed);
						break;

					default:
						break;
					}

					workItem->Reset();
					SQueuedWorkItem::Free(workItem);
				}

				const auto now = std::chrono::steady_clock::now();
				for (auto& [contentInstanceId, state] : impl.contents)
				{
					(void)contentInstanceId;
					if (now < state.nextFrameTime)
					{
						continue;
					}

					const auto overdue = now - state.nextFrameTime;
					const auto frameDuration = std::max(state.frameDuration, std::chrono::milliseconds(1));
					const int delayFrame = 1 + static_cast<int>(overdue / frameDuration);
					state.nextFrameTime += frameDuration * delayFrame;
					state.lastDelayFrame.store(delayFrame, std::memory_order_relaxed);
					UpdateMaxAtomic(state.maxDelayFrame, delayFrame);
					state.content->OnFrame(delayFrame, *impl.bridge);
					state.frameCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
		});
	}

	void FContentThread::Stop()
	{
		if (!m_impl->running)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			m_impl->running = false;
		}
		m_impl->wakeCondition.notify_all();

		if (m_impl->workerThread.joinable())
		{
			m_impl->workerThread.join();
		}
	}

	Core::SContentThreadStats FContentThread::GetStatsSnapshot(const Core::FContentInstanceId contentInstanceId)
	{
		Core::SContentThreadStats stats{};
		stats.contentInstanceId = contentInstanceId;

		std::lock_guard<std::mutex> lock(m_impl->lock);
		const auto stateIt = m_impl->contents.find(contentInstanceId);
		if (stateIt == m_impl->contents.end())
		{
			return stats;
		}

		const SImpl::SPerContentState& state = stateIt->second;
		stats.contentId = state.contentId;
		stats.contentInstanceId = state.contentInstanceId;
		stats.running = m_impl->running;
		stats.enqueueEnterCallCount = state.enqueueEnterCallCount.load(std::memory_order_relaxed);
		stats.enqueueLeaveCallCount = state.enqueueLeaveCallCount.load(std::memory_order_relaxed);
		stats.enqueuePacketCallCount = state.enqueuePacketCallCount.load(std::memory_order_relaxed);
		stats.enterCount = state.enterCount.load(std::memory_order_relaxed);
		stats.leaveCount = state.leaveCount.load(std::memory_order_relaxed);
		stats.packetCount = state.packetCount.load(std::memory_order_relaxed);
		stats.frameCount = state.frameCount.load(std::memory_order_relaxed);
		stats.enterQueueDepth = state.enterQueueDepth.load(std::memory_order_relaxed);
		stats.leaveQueueDepth = state.leaveQueueDepth.load(std::memory_order_relaxed);
		stats.packetQueueDepth = state.packetQueueDepth.load(std::memory_order_relaxed);
		stats.maxEnterQueueDepth = state.maxEnterQueueDepth.load(std::memory_order_relaxed);
		stats.maxLeaveQueueDepth = state.maxLeaveQueueDepth.load(std::memory_order_relaxed);
		stats.maxPacketQueueDepth = state.maxPacketQueueDepth.load(std::memory_order_relaxed);
		stats.enqueueEnterLockWaitNs = state.enqueueEnterLockWaitNs.load(std::memory_order_relaxed);
		stats.enqueueLeaveLockWaitNs = state.enqueueLeaveLockWaitNs.load(std::memory_order_relaxed);
		stats.enqueuePacketLockWaitNs = state.enqueuePacketLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueueEnterLockWaitNs = state.maxEnqueueEnterLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueueLeaveLockWaitNs = state.maxEnqueueLeaveLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueuePacketLockWaitNs = state.maxEnqueuePacketLockWaitNs.load(std::memory_order_relaxed);
		stats.lastDelayFrame = state.lastDelayFrame.load(std::memory_order_relaxed);
		stats.maxDelayFrame = state.maxDelayFrame.load(std::memory_order_relaxed);
		return stats;
	}

	std::uint32_t FContentThread::GetWorkerIndex() const noexcept
	{
		return m_impl->workerIndex;
	}

	void FContentThread::EnqueueEnter(Core::SContentLifecycleEvent event)
	{
		auto stateIt = m_impl->contents.find(event.contentInstanceId);
		if (stateIt == m_impl->contents.end())
		{
			return;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.enqueueEnterCallCount.fetch_add(1, std::memory_order_relaxed);
		const std::uint64_t queueDepth =
			state.enterQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(state.maxEnterQueueDepth, queueDepth);

		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Enter;
		workItem->lifecycleEvent = std::move(event);
		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		m_impl->workQueueLockFree.Enqueue(workItem);
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueueLeave(Core::SContentLifecycleEvent event)
	{
		auto stateIt = m_impl->contents.find(event.contentInstanceId);
		if (stateIt == m_impl->contents.end())
		{
			return;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.enqueueLeaveCallCount.fetch_add(1, std::memory_order_relaxed);
		const std::uint64_t queueDepth =
			state.leaveQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(state.maxLeaveQueueDepth, queueDepth);

		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Leave;
		workItem->lifecycleEvent = std::move(event);
		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		m_impl->workQueueLockFree.Enqueue(workItem);
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueuePacket(Core::FOwnedPacketEnvelope&& packet)
	{
		auto stateIt = m_impl->contents.find(packet.contentInstanceId);
		if (stateIt == m_impl->contents.end())
		{
			return;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const std::uint64_t queueDepth =
			state.packetQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(state.maxPacketQueueDepth, queueDepth);

		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Packet;
		workItem->packet = std::move(packet);
		if (m_impl->config.enableTraceLogging)
		{
			std::ostringstream oss;
			oss << "worker enqueue packet. workerIndex=" << m_impl->workerIndex
				<< " sessionId=" << workItem->packet.sessionId
				<< " opcode=" << workItem->packet.opcode
				<< " routeGeneration=" << workItem->packet.routeGeneration
				<< " contentId=" << state.contentId
				<< " contentInstanceId=" << state.contentInstanceId
				<< " queueDepth=" << queueDepth;
			TraceThread(m_impl->config, workItem->packet.sessionId, oss.str());
		}

		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		m_impl->workQueueLockFree.Enqueue(workItem);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		m_impl->wakeCondition.notify_one();
	}
}
