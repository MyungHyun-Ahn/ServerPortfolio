#include "Pch.h"

#include "ContentsRuntime/Threading/FContentThread.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "ContentsRuntime/Core/IContent.h"

namespace ContentsRuntime::Threading
{
	namespace
	{
		inline constexpr std::size_t kMailboxBatchSize = 64;

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
			std::chrono::steady_clock::time_point enqueuedAt{};
			Core::SContentLifecycleEvent lifecycleEvent{};
			Core::FOwnedPacketEnvelope packet{};

			void Reset() noexcept
			{
				kind = EQueuedWorkKind::Packet;
				enqueuedAt = std::chrono::steady_clock::time_point{};
				lifecycleEvent = {};
				packet = {};
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
		struct SMailbox
		{
			std::mutex lock;
			std::deque<SQueuedWorkItem*> items;
			bool readyQueued = false;
		};

		struct SPerContentState
		{
			Core::IContent* content = nullptr;
			Core::FContentId contentId = Core::kInvalidContentId;
			Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
			std::chrono::milliseconds frameDuration{ 33 };
			std::chrono::steady_clock::time_point nextFrameTime{};
			SMailbox mailbox;
			std::atomic<std::uint64_t> inFlightCallbackCount = 0;
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
		mutable std::shared_mutex contentsLock;
		std::mutex readyLock;
		std::condition_variable wakeCondition;
		std::deque<Core::FContentInstanceId> readyContentIds;
		std::unordered_map<Core::FContentInstanceId, SPerContentState> contents;
		std::atomic<std::uint64_t> pendingWorkCount = 0;
		std::atomic<std::uint64_t> raceInjectionCounter = 0;
		std::atomic<bool> running = false;
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
		std::lock_guard<std::mutex> stateLock(m_impl->lock);
		if (m_impl->running.load(std::memory_order_relaxed))
		{
			return false;
		}

		const Core::FContentInstanceId contentInstanceId = content.GetContentInstanceId();
		if (content.GetContentId() == Core::kInvalidContentId ||
			contentInstanceId == Core::kInvalidContentInstanceId)
		{
			return false;
		}

		std::unique_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
		if (m_impl->contents.contains(contentInstanceId))
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
		if (m_impl->running.exchange(true, std::memory_order_relaxed))
		{
			return;
		}

		{
			std::unique_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
			const auto now = std::chrono::steady_clock::now();
			for (auto& [contentInstanceId, state] : m_impl->contents)
			{
				(void)contentInstanceId;
				state.nextFrameTime = now + state.frameDuration;
			}
		}

		m_impl->workerThread = std::thread([this]()
		{
			SImpl& impl = *m_impl;

			auto pushReadyContent = [&](const Core::FContentInstanceId contentInstanceId)
			{
				std::lock_guard<std::mutex> readyLock(impl.readyLock);
				impl.readyContentIds.push_back(contentInstanceId);
				impl.wakeCondition.notify_one();
			};

			auto processQueuedWork = [&](SImpl::SPerContentState& state, SQueuedWorkItem* workItem)
			{
				if (workItem == nullptr || state.content == nullptr)
				{
					return;
				}

				const auto queueWaitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - workItem->enqueuedAt).count();
				const auto pendingWorkCount =
					impl.pendingWorkCount.fetch_sub(1, std::memory_order_relaxed) - 1;

				state.inFlightCallbackCount.fetch_add(1, std::memory_order_relaxed);
				switch (workItem->kind)
				{
				case EQueuedWorkKind::Enter:
					if (impl.config.enableTraceLogging)
					{
						std::ostringstream oss;
						oss << "worker execute enter. workerIndex=" << impl.workerIndex
							<< " sessionId=" << workItem->lifecycleEvent.sessionId
							<< " routeGeneration=" << workItem->lifecycleEvent.routeGeneration
							<< " contentInstanceId=" << workItem->lifecycleEvent.contentInstanceId
							<< " queueWaitMs=" << queueWaitMs
							<< " pendingWorkCount=" << pendingWorkCount
							<< " enterQueueDepth=" << state.enterQueueDepth.load(std::memory_order_relaxed)
							<< " leaveQueueDepth=" << state.leaveQueueDepth.load(std::memory_order_relaxed)
							<< " packetQueueDepth=" << state.packetQueueDepth.load(std::memory_order_relaxed);
						TraceThread(impl.config, workItem->lifecycleEvent.sessionId, oss.str());
					}

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
						if (impl.config.enableTraceLogging)
						{
							std::ostringstream oss;
							oss << "worker invoke enter completion callback. workerIndex=" << impl.workerIndex
								<< " sessionId=" << workItem->lifecycleEvent.sessionId
								<< " routeGeneration=" << workItem->lifecycleEvent.routeGeneration
								<< " contentInstanceId=" << workItem->lifecycleEvent.contentInstanceId;
							TraceThread(impl.config, workItem->lifecycleEvent.sessionId, oss.str());
						}
						workItem->lifecycleEvent.completionCallback();
					}
					state.enterCount.fetch_add(1, std::memory_order_relaxed);
					state.enterQueueDepth.fetch_sub(1, std::memory_order_relaxed);
					break;

				case EQueuedWorkKind::Leave:
					if (impl.config.enableTraceLogging)
					{
						std::ostringstream oss;
						oss << "worker execute leave. workerIndex=" << impl.workerIndex
							<< " sessionId=" << workItem->lifecycleEvent.sessionId
							<< " routeGeneration=" << workItem->lifecycleEvent.routeGeneration
							<< " contentInstanceId=" << workItem->lifecycleEvent.contentInstanceId
							<< " queueWaitMs=" << queueWaitMs
							<< " pendingWorkCount=" << pendingWorkCount
							<< " enterQueueDepth=" << state.enterQueueDepth.load(std::memory_order_relaxed)
							<< " leaveQueueDepth=" << state.leaveQueueDepth.load(std::memory_order_relaxed)
							<< " packetQueueDepth=" << state.packetQueueDepth.load(std::memory_order_relaxed);
						TraceThread(impl.config, workItem->lifecycleEvent.sessionId, oss.str());
					}

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
							<< " payloadBytes=" << workItem->packet.payload.size()
							<< " queueWaitMs=" << queueWaitMs
							<< " pendingWorkCount=" << pendingWorkCount
							<< " enterQueueDepth=" << state.enterQueueDepth.load(std::memory_order_relaxed)
							<< " leaveQueueDepth=" << state.leaveQueueDepth.load(std::memory_order_relaxed)
							<< " packetQueueDepth=" << state.packetQueueDepth.load(std::memory_order_relaxed);
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
				}

				state.inFlightCallbackCount.fetch_sub(1, std::memory_order_relaxed);
				workItem->Reset();
				SQueuedWorkItem::Free(workItem);
			};

			auto processContentMailbox = [&](const Core::FContentInstanceId contentInstanceId)
			{
				SImpl::SPerContentState* state = nullptr;
				{
					std::shared_lock<std::shared_mutex> contentsLock(impl.contentsLock);
					const auto stateIt = impl.contents.find(contentInstanceId);
					if (stateIt == impl.contents.end() || stateIt->second.content == nullptr)
					{
						return;
					}

					state = &stateIt->second;
				}

				std::vector<SQueuedWorkItem*> workItems;
				workItems.reserve(kMailboxBatchSize);
				{
					std::lock_guard<std::mutex> mailboxLock(state->mailbox.lock);
					while (!state->mailbox.items.empty() && workItems.size() < kMailboxBatchSize)
					{
						workItems.push_back(state->mailbox.items.front());
						state->mailbox.items.pop_front();
					}
				}

				for (SQueuedWorkItem* workItem : workItems)
				{
					processQueuedWork(*state, workItem);
				}

				bool requeue = false;
				{
					std::lock_guard<std::mutex> mailboxLock(state->mailbox.lock);
					if (state->mailbox.items.empty())
					{
						state->mailbox.readyQueued = false;
					}
					else
					{
						requeue = true;
					}
				}

				if (requeue)
				{
					pushReadyContent(contentInstanceId);
				}
			};

			auto processDueFrames = [&]() -> bool
			{
				const auto now = std::chrono::steady_clock::now();
				std::vector<Core::FContentInstanceId> dueFrameContentInstanceIds;
				{
					std::shared_lock<std::shared_mutex> contentsLock(impl.contentsLock);
					dueFrameContentInstanceIds.reserve(impl.contents.size());
					for (const auto& [contentInstanceId, state] : impl.contents)
					{
						if (state.content == nullptr)
						{
							continue;
						}

						if (now >= state.nextFrameTime)
						{
							dueFrameContentInstanceIds.push_back(contentInstanceId);
						}
					}
				}

				bool processed = false;
				for (const Core::FContentInstanceId contentInstanceId : dueFrameContentInstanceIds)
				{
					Core::IContent* content = nullptr;
					SImpl::SPerContentState* state = nullptr;
					int delayFrame = 0;
					{
						std::unique_lock<std::shared_mutex> contentsLock(impl.contentsLock);
						const auto stateIt = impl.contents.find(contentInstanceId);
						if (stateIt == impl.contents.end() || stateIt->second.content == nullptr)
						{
							continue;
						}

						state = &stateIt->second;
						content = state->content;
						const auto frameNow = std::chrono::steady_clock::now();
						if (frameNow < state->nextFrameTime)
						{
							continue;
						}

						const auto frameDuration = std::max(state->frameDuration, std::chrono::milliseconds(1));
						const auto overdue = frameNow - state->nextFrameTime;
						delayFrame = 1 + static_cast<int>(overdue / frameDuration);
						state->nextFrameTime += frameDuration * delayFrame;
						state->lastDelayFrame.store(delayFrame, std::memory_order_relaxed);
						UpdateMaxAtomic(state->maxDelayFrame, delayFrame);
						state->inFlightCallbackCount.fetch_add(1, std::memory_order_relaxed);
					}

					const auto frameBegin = std::chrono::steady_clock::now();
					content->OnFrame(delayFrame, *impl.bridge);
					const auto frameElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - frameBegin).count();
					state->frameCount.fetch_add(1, std::memory_order_relaxed);
					state->inFlightCallbackCount.fetch_sub(1, std::memory_order_relaxed);
					if (impl.config.enableTraceLogging && frameElapsedMs >= 10)
					{
						std::ostringstream oss;
						oss << "worker slow frame. workerIndex=" << impl.workerIndex
							<< " contentInstanceId=" << contentInstanceId
							<< " contentId=" << state->contentId
							<< " delayFrame=" << delayFrame
							<< " frameElapsedMs=" << frameElapsedMs
							<< " pendingWorkCount=" << impl.pendingWorkCount.load(std::memory_order_relaxed)
							<< " enterQueueDepth=" << state->enterQueueDepth.load(std::memory_order_relaxed)
							<< " leaveQueueDepth=" << state->leaveQueueDepth.load(std::memory_order_relaxed)
							<< " packetQueueDepth=" << state->packetQueueDepth.load(std::memory_order_relaxed);
						TraceThread(impl.config, 0, oss.str());
					}
					processed = true;
				}

				return processed;
			};

			auto computeNextFrameTime = [&]() -> std::chrono::steady_clock::time_point
			{
				std::shared_lock<std::shared_mutex> contentsLock(impl.contentsLock);
				std::chrono::steady_clock::time_point nextWakeTime = std::chrono::steady_clock::time_point::max();
				for (const auto& [contentInstanceId, state] : impl.contents)
				{
					(void)contentInstanceId;
					if (state.content == nullptr)
					{
						continue;
					}

					nextWakeTime = std::min(nextWakeTime, state.nextFrameTime);
				}

				return nextWakeTime;
			};

			while (impl.running.load(std::memory_order_relaxed))
			{
				Core::FContentInstanceId readyContentInstanceId = Core::kInvalidContentInstanceId;
				{
					std::lock_guard<std::mutex> readyLock(impl.readyLock);
					if (!impl.readyContentIds.empty())
					{
						readyContentInstanceId = impl.readyContentIds.front();
						impl.readyContentIds.pop_front();
					}
				}

				if (readyContentInstanceId != Core::kInvalidContentInstanceId)
				{
					processContentMailbox(readyContentInstanceId);
					processDueFrames();
					continue;
				}

				if (processDueFrames())
				{
					continue;
				}

				const auto nextWakeTime = computeNextFrameTime();
				std::unique_lock<std::mutex> readyLock(impl.readyLock);
				if (!impl.running.load(std::memory_order_relaxed))
				{
					break;
				}

				if (!impl.readyContentIds.empty())
				{
					continue;
				}

				if (nextWakeTime == std::chrono::steady_clock::time_point::max())
				{
					impl.wakeCondition.wait(
						readyLock,
						[&impl]()
						{
							return !impl.running.load(std::memory_order_relaxed) || !impl.readyContentIds.empty();
						});
				}
				else
				{
					impl.wakeCondition.wait_until(
						readyLock,
						nextWakeTime,
						[&impl]()
						{
							return !impl.running.load(std::memory_order_relaxed) || !impl.readyContentIds.empty();
						});
				}
			}
		});
	}

	void FContentThread::Stop()
	{
		if (!m_impl->running.exchange(false, std::memory_order_relaxed))
		{
			return;
		}

		m_impl->wakeCondition.notify_all();
		if (m_impl->workerThread.joinable())
		{
			m_impl->workerThread.join();
		}

		std::vector<SQueuedWorkItem*> workItems;
		{
			std::unique_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
			for (auto& [contentInstanceId, state] : m_impl->contents)
			{
				(void)contentInstanceId;
				std::lock_guard<std::mutex> mailboxLock(state.mailbox.lock);
				while (!state.mailbox.items.empty())
				{
					workItems.push_back(state.mailbox.items.front());
					state.mailbox.items.pop_front();
				}
				state.mailbox.readyQueued = false;
			}
		}

		for (SQueuedWorkItem* workItem : workItems)
		{
			if (workItem == nullptr)
			{
				continue;
			}

			workItem->Reset();
			SQueuedWorkItem::Free(workItem);
		}

		{
			std::lock_guard<std::mutex> readyLock(m_impl->readyLock);
			m_impl->readyContentIds.clear();
		}
		m_impl->pendingWorkCount.store(0, std::memory_order_relaxed);
	}

	Core::SContentThreadStats FContentThread::GetStatsSnapshot(const Core::FContentInstanceId contentInstanceId)
	{
		Core::SContentThreadStats stats{};
		stats.contentInstanceId = contentInstanceId;

		std::shared_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
		const auto stateIt = m_impl->contents.find(contentInstanceId);
		if (stateIt == m_impl->contents.end())
		{
			return stats;
		}

		const SImpl::SPerContentState& state = stateIt->second;
		stats.contentId = state.contentId;
		stats.contentInstanceId = state.contentInstanceId;
		stats.running = m_impl->running.load(std::memory_order_relaxed);
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

	std::uint64_t FContentThread::GetApproxPendingWorkCount() const noexcept
	{
		return m_impl->pendingWorkCount.load(std::memory_order_relaxed);
	}

	bool FContentThread::EnqueueEnter(Core::SContentLifecycleEvent event)
	{
		std::shared_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
		auto stateIt = m_impl->contents.find(event.contentInstanceId);
		if (stateIt == m_impl->contents.end() || stateIt->second.content == nullptr)
		{
			return false;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.enqueueEnterCallCount.fetch_add(1, std::memory_order_relaxed);
		const std::uint64_t queueDepth =
			state.enterQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(state.maxEnterQueueDepth, queueDepth);

		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Enter;
		workItem->enqueuedAt = std::chrono::steady_clock::now();
		workItem->lifecycleEvent = std::move(event);

		bool shouldWake = false;
		const auto mailboxLockWaitStart = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> mailboxLock(state.mailbox.lock);
			const auto waitNs = ToNanoseconds(std::chrono::steady_clock::now() - mailboxLockWaitStart);
			state.enqueueEnterLockWaitNs.fetch_add(waitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(state.maxEnqueueEnterLockWaitNs, waitNs);
			state.mailbox.items.push_back(workItem);
			if (!state.mailbox.readyQueued)
			{
				state.mailbox.readyQueued = true;
				shouldWake = true;
			}
		}

		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		if (shouldWake)
		{
			std::lock_guard<std::mutex> readyLock(m_impl->readyLock);
			m_impl->readyContentIds.push_back(state.contentInstanceId);
		}
		if (shouldWake)
		{
			m_impl->wakeCondition.notify_one();
		}
		return true;
	}

	bool FContentThread::EnqueueLeave(Core::SContentLifecycleEvent event)
	{
		std::shared_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
		auto stateIt = m_impl->contents.find(event.contentInstanceId);
		if (stateIt == m_impl->contents.end() || stateIt->second.content == nullptr)
		{
			return false;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.enqueueLeaveCallCount.fetch_add(1, std::memory_order_relaxed);
		const std::uint64_t queueDepth =
			state.leaveQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(state.maxLeaveQueueDepth, queueDepth);

		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Leave;
		workItem->enqueuedAt = std::chrono::steady_clock::now();
		workItem->lifecycleEvent = std::move(event);

		bool shouldWake = false;
		const auto mailboxLockWaitStart = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> mailboxLock(state.mailbox.lock);
			const auto waitNs = ToNanoseconds(std::chrono::steady_clock::now() - mailboxLockWaitStart);
			state.enqueueLeaveLockWaitNs.fetch_add(waitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(state.maxEnqueueLeaveLockWaitNs, waitNs);
			state.mailbox.items.push_back(workItem);
			if (!state.mailbox.readyQueued)
			{
				state.mailbox.readyQueued = true;
				shouldWake = true;
			}
		}

		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		if (shouldWake)
		{
			std::lock_guard<std::mutex> readyLock(m_impl->readyLock);
			m_impl->readyContentIds.push_back(state.contentInstanceId);
		}
		if (shouldWake)
		{
			m_impl->wakeCondition.notify_one();
		}
		return true;
	}

	bool FContentThread::EnqueuePacket(Core::FOwnedPacketEnvelope packet)
	{
		std::shared_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
		auto stateIt = m_impl->contents.find(packet.contentInstanceId);
		if (stateIt == m_impl->contents.end() || stateIt->second.content == nullptr)
		{
			return false;
		}

		SImpl::SPerContentState& state = stateIt->second;
		state.enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const std::uint64_t queueDepth =
			state.packetQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
		UpdateMaxAtomic(state.maxPacketQueueDepth, queueDepth);

		SQueuedWorkItem* workItem = SQueuedWorkItem::Alloc();
		workItem->kind = EQueuedWorkKind::Packet;
		workItem->enqueuedAt = std::chrono::steady_clock::now();
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

		bool shouldWake = false;
		const auto mailboxLockWaitStart = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> mailboxLock(state.mailbox.lock);
			const auto waitNs = ToNanoseconds(std::chrono::steady_clock::now() - mailboxLockWaitStart);
			state.enqueuePacketLockWaitNs.fetch_add(waitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(state.maxEnqueuePacketLockWaitNs, waitNs);
			state.mailbox.items.push_back(workItem);
			if (!state.mailbox.readyQueued)
			{
				state.mailbox.readyQueued = true;
				shouldWake = true;
			}
		}

		m_impl->pendingWorkCount.fetch_add(1, std::memory_order_relaxed);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		if (shouldWake)
		{
			std::lock_guard<std::mutex> readyLock(m_impl->readyLock);
			m_impl->readyContentIds.push_back(state.contentInstanceId);
		}
		if (shouldWake)
		{
			m_impl->wakeCondition.notify_one();
		}
		return true;
	}

	bool FContentThread::EnqueueMoveTransition(
		Core::SContentLifecycleEvent sourceLeaveEvent,
		Core::SContentLifecycleEvent targetEnterEvent)
	{
		std::shared_lock<std::shared_mutex> contentsLock(m_impl->contentsLock);
		const auto sourceIt = m_impl->contents.find(sourceLeaveEvent.contentInstanceId);
		const auto targetIt = m_impl->contents.find(targetEnterEvent.contentInstanceId);
		if (sourceIt == m_impl->contents.end() ||
			targetIt == m_impl->contents.end() ||
			sourceIt->second.content == nullptr ||
			targetIt->second.content == nullptr)
		{
			return false;
		}

		auto sourceCompletion = std::move(sourceLeaveEvent.completionCallback);
		sourceLeaveEvent.completionCallback =
			[this,
			targetEnterEvent = std::move(targetEnterEvent),
			sourceCompletion = std::move(sourceCompletion)]() mutable
		{
			if (sourceCompletion)
			{
				sourceCompletion();
			}

			auto fallbackCompletion = targetEnterEvent.completionCallback;
			if (!this->EnqueueEnter(std::move(targetEnterEvent)) && fallbackCompletion)
			{
				fallbackCompletion();
			}
		};

		return EnqueueLeave(std::move(sourceLeaveEvent));
	}
}
