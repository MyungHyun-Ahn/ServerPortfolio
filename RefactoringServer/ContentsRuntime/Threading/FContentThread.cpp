#include "Pch.h"

#include "ContentsRuntime/Threading/FContentThread.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "ContentsRuntime/Core/IContent.h"

namespace ContentsRuntime::Threading
{
	namespace
	{
		inline constexpr bool kUseLockFreePacketInboxPrototype = true;

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

		struct SQueuedOwnedPacket
		{
			Core::FOwnedPacketEnvelope packet{};

			void Reset() noexcept
			{
				packet.sessionId = 0;
				packet.opcode = 0;
				packet.payload.clear();
			}

			USE_TLS_POOL_WITH_INIT(SQueuedOwnedPacket, s_pool, Reset)
		};
	}

	struct FContentThread::SImpl
	{
		Core::IContent* content = nullptr;
		Bridge::IContentBridge* bridge = nullptr;
		Core::SContentRuntimeConfig config{};
		std::thread workerThread;
		std::mutex lock;
		std::condition_variable wakeCondition;
		std::deque<std::uint64_t> enterQueue;
		std::deque<std::uint64_t> leaveQueue;
		std::deque<Core::FOwnedPacketEnvelope> packetQueue;
		NetworkLib::Containers::FLockFreeQueue<SQueuedOwnedPacket*> packetQueueLockFree;
		bool running = false;
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
				std::deque<std::uint64_t> enterQueue;
				std::deque<std::uint64_t> leaveQueue;
				std::deque<Core::FOwnedPacketEnvelope> packetQueue;
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
								!impl.enterQueue.empty() ||
								!impl.leaveQueue.empty() ||
								impl.packetQueueDepth.load(std::memory_order_relaxed) > 0;
						});

					if (!impl.running)
					{
						break;
					}

					enterQueue.swap(impl.enterQueue);
					leaveQueue.swap(impl.leaveQueue);
					if constexpr (!kUseLockFreePacketInboxPrototype)
					{
						packetQueue.swap(impl.packetQueue);
					}
					impl.enterQueueDepth.store(static_cast<std::uint64_t>(impl.enterQueue.size()), std::memory_order_relaxed);
					impl.leaveQueueDepth.store(static_cast<std::uint64_t>(impl.leaveQueue.size()), std::memory_order_relaxed);
					if constexpr (!kUseLockFreePacketInboxPrototype)
					{
						impl.packetQueueDepth.store(static_cast<std::uint64_t>(impl.packetQueue.size()), std::memory_order_relaxed);
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

				if constexpr (kUseLockFreePacketInboxPrototype)
				{
					std::uint64_t drainedCount = 0;
					SQueuedOwnedPacket* queuedPacket = nullptr;
					RunRaceInjection(impl.config, impl.raceInjectionCounter);
					while (impl.packetQueueLockFree.Dequeue(queuedPacket))
					{
						if (queuedPacket == nullptr)
						{
							continue;
						}

						packetQueue.push_back(std::move(queuedPacket->packet));
						queuedPacket->Reset();
						SQueuedOwnedPacket::Free(queuedPacket);
						++drainedCount;
						RunRaceInjection(impl.config, impl.raceInjectionCounter);
					}

					if (drainedCount > 0)
					{
						const std::uint64_t remainingDepth =
							impl.packetQueueDepth.fetch_sub(drainedCount, std::memory_order_relaxed) - drainedCount;
						(void)remainingDepth;
					}
				}

				for (const std::uint64_t sessionId : enterQueue)
				{
					impl.content->OnEnter(sessionId, *impl.bridge);
					impl.enterCount.fetch_add(1, std::memory_order_relaxed);
				}

				for (const std::uint64_t sessionId : leaveQueue)
				{
					impl.content->OnLeave(sessionId, *impl.bridge);
					impl.leaveCount.fetch_add(1, std::memory_order_relaxed);
				}

				for (Core::FOwnedPacketEnvelope& packet : packetQueue)
				{
					impl.content->OnPacket(
						packet.sessionId,
						packet.opcode,
						std::span<const char>(packet.payload.data(), packet.payload.size()),
						*impl.bridge);
					impl.packetCount.fetch_add(1, std::memory_order_relaxed);
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

	void FContentThread::EnqueueEnter(std::uint64_t sessionId)
	{
		m_impl->enqueueEnterCallCount.fetch_add(1, std::memory_order_relaxed);
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
			m_impl->enqueueEnterLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxEnqueueEnterLockWaitNs, lockWaitNs);
			m_impl->enterQueue.push_back(sessionId);
			const std::uint64_t queueDepth = static_cast<std::uint64_t>(m_impl->enterQueue.size());
			m_impl->enterQueueDepth.store(queueDepth, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxEnterQueueDepth, queueDepth);
		}
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueueLeave(std::uint64_t sessionId)
	{
		m_impl->enqueueLeaveCallCount.fetch_add(1, std::memory_order_relaxed);
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
			m_impl->enqueueLeaveLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxEnqueueLeaveLockWaitNs, lockWaitNs);
			m_impl->leaveQueue.push_back(sessionId);
			const std::uint64_t queueDepth = static_cast<std::uint64_t>(m_impl->leaveQueue.size());
			m_impl->leaveQueueDepth.store(queueDepth, std::memory_order_relaxed);
			UpdateMaxAtomic(m_impl->maxLeaveQueueDepth, queueDepth);
		}
		m_impl->wakeCondition.notify_one();
	}

	void FContentThread::EnqueuePacket(Core::FOwnedPacketEnvelope&& packet)
	{
		m_impl->enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		if constexpr (kUseLockFreePacketInboxPrototype)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			const std::uint64_t queueDepth =
				m_impl->packetQueueDepth.fetch_add(1, std::memory_order_relaxed) + 1;
			UpdateMaxAtomic(m_impl->maxPacketQueueDepth, queueDepth);
			SQueuedOwnedPacket* queuedPacket = SQueuedOwnedPacket::Alloc();
			queuedPacket->packet = std::move(packet);
			m_impl->packetQueueLockFree.Enqueue(queuedPacket);
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		}
		else
		{
			const auto lockWaitStart = std::chrono::steady_clock::now();
			{
				std::lock_guard<std::mutex> lock(m_impl->lock);
				const std::uint64_t lockWaitNs = ToNanoseconds(std::chrono::steady_clock::now() - lockWaitStart);
				m_impl->enqueuePacketLockWaitNs.fetch_add(lockWaitNs, std::memory_order_relaxed);
				UpdateMaxAtomic(m_impl->maxEnqueuePacketLockWaitNs, lockWaitNs);
				m_impl->packetQueue.push_back(std::move(packet));
				const std::uint64_t queueDepth = static_cast<std::uint64_t>(m_impl->packetQueue.size());
				m_impl->packetQueueDepth.store(queueDepth, std::memory_order_relaxed);
				UpdateMaxAtomic(m_impl->maxPacketQueueDepth, queueDepth);
			}
		}
		m_impl->wakeCondition.notify_one();
	}
}
