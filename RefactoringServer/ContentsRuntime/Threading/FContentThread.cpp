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

		void TraceThread(const Core::SContentRuntimeConfig& config, const std::uint64_t sessionId, const std::string& message)
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
		Core::IContent* content = nullptr;
		Bridge::IContentBridge* bridge = nullptr;
		Core::SContentRuntimeConfig config{};
		std::thread workerThread;
		std::mutex lock;
		std::condition_variable wakeCondition;
		NetworkLib::Containers::FLockFreeQueue<SQueuedWorkItem*> workQueueLockFree;
		bool running = false;
		std::atomic<std::uint64_t> enqueueEnterCallCount = 0;
		std::atomic<std::uint64_t> enqueueLeaveCallCount = 0;
		std::atomic<std::uint64_t> enqueuePacketCallCount = 0;
		std::atomic<std::uint64_t> enterCount = 0;
		std::atomic<std::uint64_t> leaveCount = 0;
		std::atomic<std::uint64_t> packetCount = 0;
		std::atomic<std::uint64_t> frameCount = 0;
		std::atomic<std::uint64_t> pendingWorkCount = 0;
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
		std::atomic<std::uint64_t> raceInjectionCounter = 0;
	};

	FContentThread::FContentThread(Core::IContent& content, Bridge::IContentBridge& bridge, const Core::SContentRuntimeConfig& config)
		: m_impl(std::make_unique<SImpl>())
	{
		m_impl->content = &content;
		m_impl->bridge = &bridge;
		m_impl->config = config;
	}

	FContentThread::~FContentThread()
	{
		Stop();
	}

	void FContentThread::Start()
	{
		if (m_impl->running)
		{
			return;
		}

		m_impl->running = true;
		m_impl->workerThread = std::thread([this]()
		{
			SImpl& impl = *m_impl;
			const std::uint32_t targetFps = std::max<std::uint32_t>(1u, impl.content->GetTargetFps());
			const auto frameDuration = std::chrono::milliseconds(std::max<std::int64_t>(1, 1000 / static_cast<std::int64_t>(targetFps)));
			auto nextFrameTime = std::chrono::steady_clock::now() + frameDuration;

			while (true)
			{
				std::vector<SQueuedWorkItem*> workItems;
				int delayFrame = 1;
				bool shouldRunFrame = false;

				{
					std::unique_lock<std::mutex> lock(impl.lock);
					impl.wakeCondition.wait_until(
						lock,
						nextFrameTime,
						[&impl]()
						{
							return !impl.running ||
								impl.pendingWorkCount.load(std::memory_order_relaxed) > 0;
						});

					if (!impl.running)
					{
						break;
					}

					const auto now = std::chrono::steady_clock::now();
					if (now >= nextFrameTime)
					{
						const auto overdue = now - nextFrameTime;
						delayFrame = 1 + static_cast<int>(overdue / frameDuration);
						nextFrameTime += frameDuration * delayFrame;
						shouldRunFrame = true;
					}
				}

				SQueuedWorkItem* queuedWorkItem = nullptr;
				RunRaceInjection(impl.config, impl.raceInjectionCounter);
				while (impl.workQueueLockFree.Dequeue(queuedWorkItem))
				{
					if (queuedWorkItem == nullptr)
					{
						continue;
					}

					workItems.push_back(queuedWorkItem);
					RunRaceInjection(impl.config, impl.raceInjectionCounter);
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

					switch (workItem->kind)
					{
					case EQueuedWorkKind::Enter:
						impl.content->OnEnter(workItem->lifecycleEvent.sessionId, workItem->lifecycleEvent.routeGeneration, *impl.bridge);
						if (workItem->lifecycleEvent.completionFlag != nullptr)
						{
							workItem->lifecycleEvent.completionFlag->store(true, std::memory_order_release);
						}
						if (workItem->lifecycleEvent.completionCallback)
						{
							workItem->lifecycleEvent.completionCallback();
						}
						impl.enterCount.fetch_add(1, std::memory_order_relaxed);
						impl.enterQueueDepth.fetch_sub(1, std::memory_order_relaxed);
						break;
					case EQueuedWorkKind::Leave:
						impl.content->OnLeave(workItem->lifecycleEvent.sessionId, workItem->lifecycleEvent.routeGeneration, *impl.bridge);
						if (workItem->lifecycleEvent.completionFlag != nullptr)
						{
							workItem->lifecycleEvent.completionFlag->store(true, std::memory_order_release);
						}
						if (workItem->lifecycleEvent.completionCallback)
						{
							workItem->lifecycleEvent.completionCallback();
						}
						impl.leaveCount.fetch_add(1, std::memory_order_relaxed);
						impl.leaveQueueDepth.fetch_sub(1, std::memory_order_relaxed);
						break;
					case EQueuedWorkKind::Packet:
						if (impl.config.enableTraceLogging)
						{
							std::ostringstream oss;
							oss << "thread dequeue packet. sessionId=" << workItem->packet.sessionId
								<< " opcode=" << workItem->packet.opcode
								<< " routeGeneration=" << workItem->packet.routeGeneration
								<< " contentId=" << impl.content->GetContentId()
								<< " contentInstanceId=" << impl.content->GetContentInstanceId()
								<< " payloadBytes=" << workItem->packet.payload.size();
							TraceThread(impl.config, workItem->packet.sessionId, oss.str());
						}
						impl.content->OnPacket(
							workItem->packet.sessionId,
							workItem->packet.routeGeneration,
							workItem->packet.opcode,
							std::span<const char>(workItem->packet.payload.data(), workItem->packet.payload.size()),
							*impl.bridge);
						impl.packetCount.fetch_add(1, std::memory_order_relaxed);
						impl.packetQueueDepth.fetch_sub(1, std::memory_order_relaxed);
						break;
					default:
						break;
					}

					workItem->Reset();
					SQueuedWorkItem::Free(workItem);
				}

				if (shouldRunFrame)
				{
					impl.lastDelayFrame.store(delayFrame, std::memory_order_relaxed);
					UpdateMaxAtomic(impl.maxDelayFrame, delayFrame);
					impl.content->OnFrame(delayFrame, *impl.bridge);
					impl.frameCount.fetch_add(1, std::memory_order_relaxed);
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

	Core::SContentThreadStats FContentThread::GetStatsSnapshot()
	{
		Core::SContentThreadStats stats{};
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			stats.contentId = m_impl->content != nullptr ? m_impl->content->GetContentId() : Core::kInvalidContentId;
			stats.contentInstanceId =
				m_impl->content != nullptr ? m_impl->content->GetContentInstanceId() : Core::kInvalidContentInstanceId;
			stats.running = m_impl->running;
		}

		stats.enqueueEnterCallCount = m_impl->enqueueEnterCallCount.load(std::memory_order_relaxed);
		stats.enqueueLeaveCallCount = m_impl->enqueueLeaveCallCount.load(std::memory_order_relaxed);
		stats.enqueuePacketCallCount = m_impl->enqueuePacketCallCount.load(std::memory_order_relaxed);
		stats.enterCount = m_impl->enterCount.load(std::memory_order_relaxed);
		stats.leaveCount = m_impl->leaveCount.load(std::memory_order_relaxed);
		stats.packetCount = m_impl->packetCount.load(std::memory_order_relaxed);
		stats.frameCount = m_impl->frameCount.load(std::memory_order_relaxed);
		stats.enterQueueDepth = m_impl->enterQueueDepth.load(std::memory_order_relaxed);
		stats.leaveQueueDepth = m_impl->leaveQueueDepth.load(std::memory_order_relaxed);
		stats.packetQueueDepth = m_impl->packetQueueDepth.load(std::memory_order_relaxed);
		stats.maxEnterQueueDepth = m_impl->maxEnterQueueDepth.load(std::memory_order_relaxed);
		stats.maxLeaveQueueDepth = m_impl->maxLeaveQueueDepth.load(std::memory_order_relaxed);
		stats.maxPacketQueueDepth = m_impl->maxPacketQueueDepth.load(std::memory_order_relaxed);
		stats.enqueueEnterLockWaitNs = m_impl->enqueueEnterLockWaitNs.load(std::memory_order_relaxed);
		stats.enqueueLeaveLockWaitNs = m_impl->enqueueLeaveLockWaitNs.load(std::memory_order_relaxed);
		stats.enqueuePacketLockWaitNs = m_impl->enqueuePacketLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueueEnterLockWaitNs = m_impl->maxEnqueueEnterLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueueLeaveLockWaitNs = m_impl->maxEnqueueLeaveLockWaitNs.load(std::memory_order_relaxed);
		stats.maxEnqueuePacketLockWaitNs = m_impl->maxEnqueuePacketLockWaitNs.load(std::memory_order_relaxed);
		stats.lastDelayFrame = m_impl->lastDelayFrame.load(std::memory_order_relaxed);
		stats.maxDelayFrame = m_impl->maxDelayFrame.load(std::memory_order_relaxed);
		return stats;
	}

	void FContentThread::EnqueueEnter(Core::SContentLifecycleEvent event)
	{
		m_impl->enqueueEnterCallCount.fetch_add(1, std::memory_order_relaxed);
		const std::uint64_t queueDepth =
			m_impl->enterQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(m_impl->maxEnterQueueDepth, queueDepth);
		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Enter;
		workItem->lifecycleEvent = std::move(event);
		m_impl->workQueueLockFree.Enqueue(workItem);
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueueLeave(Core::SContentLifecycleEvent event)
	{
		m_impl->enqueueLeaveCallCount.fetch_add(1, std::memory_order_relaxed);
		const std::uint64_t queueDepth =
			m_impl->leaveQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(m_impl->maxLeaveQueueDepth, queueDepth);
		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Leave;
		workItem->lifecycleEvent = std::move(event);
		m_impl->workQueueLockFree.Enqueue(workItem);
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueuePacket(Core::FOwnedPacketEnvelope&& packet)
	{
		m_impl->enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const std::uint64_t queueDepth =
			m_impl->packetQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(m_impl->maxPacketQueueDepth, queueDepth);
		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Packet;
		workItem->packet = std::move(packet);
		if (m_impl->config.enableTraceLogging)
		{
			std::ostringstream oss;
			oss << "thread enqueue packet. sessionId=" << workItem->packet.sessionId
				<< " opcode=" << workItem->packet.opcode
				<< " routeGeneration=" << workItem->packet.routeGeneration
				<< " contentId=" << (m_impl->content != nullptr ? m_impl->content->GetContentId() : Core::kInvalidContentId)
				<< " contentInstanceId=" << (m_impl->content != nullptr ? m_impl->content->GetContentInstanceId() : Core::kInvalidContentInstanceId)
				<< " queueDepth=" << queueDepth;
			TraceThread(m_impl->config, workItem->packet.sessionId, oss.str());
		}
		m_impl->workQueueLockFree.Enqueue(workItem);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		m_impl->wakeCondition.notify_one();
	}
}
