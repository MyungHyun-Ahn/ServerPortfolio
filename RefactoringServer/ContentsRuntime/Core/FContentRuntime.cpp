#include "Pch.h"

#include "ContentsRuntime/Core/FContentRuntime.h"

#include "ContentsRuntime/Core/FContentThread.h"
#include "ContentsRuntime/Core/IContent.h"
#include "Servers/IServer.h"

#include <shared_mutex>

namespace ContentsRuntime::Core
{
	namespace
	{
		inline constexpr std::uint64_t kSessionSlotMask = 0xFFFFFFFFULL;

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

		void RunRaceInjection(const SContentRuntimeConfig& config, std::atomic<std::uint64_t>& counter) noexcept
		{
			if (!config.enableRaceInjection || config.raceInjectionPeriod == 0 || config.raceInjectionMode == ERaceInjectionMode::None)
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
			case ERaceInjectionMode::SwitchToThread:
				::SwitchToThread();
				break;
			case ERaceInjectionMode::Sleep0:
				::Sleep(0);
				break;
			case ERaceInjectionMode::Yield:
				std::this_thread::yield();
				break;
			default:
				break;
			}
		}
	}

	struct SContentSlot
	{
		std::unique_ptr<IContent> content;
		std::unique_ptr<FContentThread> thread;
	};

	struct SSessionRoute
	{
		std::uint64_t sessionId = 0;
		FContentId contentId = kInvalidContentId;
		FContentThread* thread = nullptr;
	};

	struct FContentRuntime::SImpl
	{
		NetworkLib::IServer* server = nullptr;
		mutable std::shared_mutex lock;
		std::unordered_map<FContentId, SContentSlot> contentSlots;
		std::vector<SSessionRoute> sessionRoutes;
		SContentRuntimeConfig config{};
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

	bool FContentRuntime::RegisterContent(std::unique_ptr<IContent> content)
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

		const FContentId contentId = content->GetContentId();
		if (contentId == kInvalidContentId || m_impl->contentSlots.contains(contentId))
		{
			return false;
		}

		SContentSlot slot{};
		slot.content = std::move(content);
		m_impl->contentSlots.emplace(contentId, std::move(slot));
		return true;
	}

	void FContentRuntime::SetConfig(const SContentRuntimeConfig& config)
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
		for (auto& [contentId, slot] : m_impl->contentSlots)
		{
			(void)contentId;
			slot.thread = std::make_unique<FContentThread>(*slot.content, *this, m_impl->config);
			slot.thread->Start();
		}
	}

	void FContentRuntime::Stop()
	{
		std::unordered_map<FContentId, std::unique_ptr<FContentThread>> threadsToStop;
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			for (auto& [contentId, slot] : m_impl->contentSlots)
			{
				(void)contentId;
				if (slot.thread != nullptr)
				{
					threadsToStop.emplace(contentId, std::move(slot.thread));
				}
			}
			for (SSessionRoute& route : m_impl->sessionRoutes)
			{
				route = {};
			}
			m_impl->activeSessionCount.store(0, std::memory_order_relaxed);
			m_impl->server = nullptr;
		}

		for (auto& [contentId, thread] : threadsToStop)
		{
			(void)contentId;
			thread->Stop();
		}
	}

	SContentRuntimeStats FContentRuntime::GetStatsSnapshot()
	{
		SContentRuntimeStats stats{};
		std::unordered_map<FContentId, std::uint64_t> sessionCounts;

		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
			stats.registeredContentCount = static_cast<std::uint64_t>(m_impl->contentSlots.size());
			stats.activeSessionCount = m_impl->activeSessionCount.load(std::memory_order_relaxed);
			for (const SSessionRoute& route : m_impl->sessionRoutes)
			{
				if (route.sessionId == 0 || route.contentId == kInvalidContentId)
				{
					continue;
				}

				++sessionCounts[route.contentId];
			}

			stats.contents.reserve(m_impl->contentSlots.size());
			for (auto& [contentId, slot] : m_impl->contentSlots)
			{
				SContentRuntimeContentStats contentStats{};
				contentStats.contentId = contentId;
				contentStats.activeSessionCount = sessionCounts[contentId];
				if (slot.thread != nullptr)
				{
					contentStats.threadStats = slot.thread->GetStatsSnapshot();
				}
				else
				{
					contentStats.threadStats.contentId = contentId;
				}
				stats.contents.push_back(std::move(contentStats));
			}
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

	bool FContentRuntime::EnterSession(std::uint64_t sessionId, FContentId initialContentId)
	{
		m_impl->enterSessionCallCount.fetch_add(1, std::memory_order_relaxed);
		FContentThread* targetThread = nullptr;
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

			auto contentIt = m_impl->contentSlots.find(initialContentId);
			if (contentIt == m_impl->contentSlots.end() || contentIt->second.thread == nullptr)
			{
				return false;
			}

			SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId == 0)
			{
				m_impl->activeSessionCount.fetch_add(1, std::memory_order_relaxed);
			}
			route.sessionId = sessionId;
			route.contentId = initialContentId;
			targetThread = contentIt->second.thread.get();
			route.thread = targetThread;
		}

		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		targetThread->EnqueueEnter(sessionId);
		return true;
	}

	void FContentRuntime::LeaveSession(std::uint64_t sessionId)
	{
		m_impl->leaveSessionCallCount.fetch_add(1, std::memory_order_relaxed);
		FContentThread* targetThread = nullptr;
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

			const FContentId currentContentId = route.contentId;
			targetThread = route.thread;
			route = {};
			m_impl->activeSessionCount.fetch_sub(1, std::memory_order_relaxed);

			if (targetThread == nullptr)
			{
				auto contentIt = m_impl->contentSlots.find(currentContentId);
				if (contentIt != m_impl->contentSlots.end())
				{
					targetThread = contentIt->second.thread.get();
				}
			}
		}

		if (targetThread != nullptr)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			targetThread->EnqueueLeave(sessionId);
		}
	}

	bool FContentRuntime::EnqueuePacket(std::uint64_t sessionId, std::uint16_t opcode, const char* payload, std::int32_t payloadLength)
	{
		m_impl->enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		FContentThread* targetThread = nullptr;
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		const auto lockWaitStart = std::chrono::steady_clock::now();
		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
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

			const SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId != sessionId || route.thread == nullptr)
			{
				m_impl->enqueueFailureCount.fetch_add(1, std::memory_order_relaxed);
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime enqueue failed: route mismatch or null thread.");
				}
				return false;
			}

			targetThread = route.thread;
		}

		FOwnedPacketEnvelope packet{};
		packet.sessionId = sessionId;
		packet.opcode = opcode;
		if (payload != nullptr && payloadLength > 0)
		{
			packet.payload.assign(payload, payload + payloadLength);
		}

		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		targetThread->EnqueuePacket(std::move(packet));
		return true;
	}

	bool FContentRuntime::SendRaw(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length)
	{
		NetworkLib::IServer* server = nullptr;
		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
			server = m_impl->server;
		}

		return server != nullptr && server->Send(sessionId, opcode, buffer, length);
	}

	bool FContentRuntime::MoveSession(std::uint64_t sessionId, FContentId targetContentId)
	{
		FContentThread* sourceThread = nullptr;
		FContentThread* targetThread = nullptr;
		const std::uint32_t slotIndex = DecodeSessionSlotIndex(sessionId);
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
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

			auto targetIt = m_impl->contentSlots.find(targetContentId);
			if (targetIt == m_impl->contentSlots.end() || targetIt->second.thread == nullptr)
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: target content thread missing.");
				}
				return false;
			}

			targetThread = targetIt->second.thread.get();

			SSessionRoute& route = m_impl->sessionRoutes[slotIndex];
			if (route.sessionId != sessionId)
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: route mismatch.");
				}
				return false;
			}

			if (route.thread != nullptr)
			{
				sourceThread = route.thread;
			}
			else
			{
				auto sourceIt = m_impl->contentSlots.find(route.contentId);
				if (sourceIt != m_impl->contentSlots.end() && sourceIt->second.thread != nullptr)
				{
					sourceThread = sourceIt->second.thread.get();
				}
			}

			route.contentId = targetContentId;
			route.thread = targetThread;
		}

		m_impl->moveSessionCount.fetch_add(1, std::memory_order_relaxed);

		if (sourceThread != nullptr)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			sourceThread->EnqueueLeave(sessionId);
		}
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		targetThread->EnqueueEnter(sessionId);
		return true;
	}
}
