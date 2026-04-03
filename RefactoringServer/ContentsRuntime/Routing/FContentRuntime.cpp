#include "Pch.h"

#include "ContentsRuntime/Routing/FContentRuntime.h"

#include "ContentsRuntime/Core/IContent.h"
#include "ContentsRuntime/Threading/FContentThread.h"
#include "Servers/IServer.h"

#include <shared_mutex>

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
		std::unique_ptr<Threading::FContentThread> thread;
	};

	struct SSessionRoute
	{
		std::uint64_t sessionId = 0;
		std::uint64_t routeGeneration = 0;
		Core::FContentId contentId = Core::kInvalidContentId;
		Core::FContentInstanceId contentInstanceId = Core::kInvalidContentInstanceId;
		Threading::FContentThread* thread = nullptr;
	};

	struct FContentRuntime::SImpl
	{
		NetworkLib::IServer* server = nullptr;
		mutable std::shared_mutex lock;
		std::unordered_map<Core::FContentInstanceId, SContentSlot> contentSlots;
		std::unordered_map<Core::FContentId, Core::FContentInstanceId> defaultInstanceIdsByContentId;
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
		for (auto& [contentInstanceId, slot] : m_impl->contentSlots)
		{
			(void)contentInstanceId;
			slot.thread = std::make_unique<Threading::FContentThread>(*slot.content, *this, m_impl->config);
			slot.thread->Start();
		}
	}

	void FContentRuntime::Stop()
	{
		std::unordered_map<Core::FContentInstanceId, std::unique_ptr<Threading::FContentThread>> threadsToStop;
		{
			std::unique_lock<std::shared_mutex> lock(m_impl->lock);
			for (auto& [contentInstanceId, slot] : m_impl->contentSlots)
			{
				(void)contentInstanceId;
				if (slot.thread != nullptr)
				{
					threadsToStop.emplace(contentInstanceId, std::move(slot.thread));
				}
			}
			for (SSessionRoute& route : m_impl->sessionRoutes)
			{
				route = {};
			}
			m_impl->activeSessionCount.store(0, std::memory_order_relaxed);
			m_impl->server = nullptr;
		}

		for (auto& [contentInstanceId, thread] : threadsToStop)
		{
			(void)contentInstanceId;
			thread->Stop();
		}
	}

	Core::SContentRuntimeStats FContentRuntime::GetStatsSnapshot()
	{
		Core::SContentRuntimeStats stats{};
		std::unordered_map<Core::FContentInstanceId, std::uint64_t> sessionCounts;

		{
			std::shared_lock<std::shared_mutex> lock(m_impl->lock);
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

			stats.contents.reserve(m_impl->contentSlots.size());
			for (auto& [contentInstanceId, slot] : m_impl->contentSlots)
			{
				Core::SContentRuntimeContentStats contentStats{};
				contentStats.contentId = slot.contentId;
				contentStats.contentInstanceId = contentInstanceId;
				contentStats.activeSessionCount = sessionCounts[contentInstanceId];
				if (slot.thread != nullptr)
				{
					contentStats.threadStats = slot.thread->GetStatsSnapshot();
				}
				else
				{
					contentStats.threadStats.contentId = slot.contentId;
					contentStats.threadStats.contentInstanceId = contentInstanceId;
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
		Threading::FContentThread* targetThread = nullptr;
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
			if (contentIt == m_impl->contentSlots.end() || contentIt->second.thread == nullptr)
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
			targetThread = contentIt->second.thread.get();
			route.thread = targetThread;
		}

		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		targetThread->EnqueueEnter({ sessionId, targetRouteGeneration });
		return true;
	}

	void FContentRuntime::LeaveSession(std::uint64_t sessionId)
	{
		m_impl->leaveSessionCallCount.fetch_add(1, std::memory_order_relaxed);
		Threading::FContentThread* targetThread = nullptr;
		std::uint64_t routeGeneration = 0;
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

			const Core::FContentInstanceId currentContentInstanceId = route.contentInstanceId;
			routeGeneration = route.routeGeneration;
			targetThread = route.thread;
			route = {};
			m_impl->activeSessionCount.fetch_sub(1, std::memory_order_relaxed);

			if (targetThread == nullptr)
			{
				auto contentIt = m_impl->contentSlots.find(currentContentInstanceId);
				if (contentIt != m_impl->contentSlots.end())
				{
					targetThread = contentIt->second.thread.get();
				}
			}
		}

		if (targetThread != nullptr)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			targetThread->EnqueueLeave({ sessionId, routeGeneration });
		}
	}

	bool FContentRuntime::EnqueuePacket(std::uint64_t sessionId, std::uint16_t opcode, const char* payload, std::int32_t payloadLength)
	{
		m_impl->enqueuePacketCallCount.fetch_add(1, std::memory_order_relaxed);
		Threading::FContentThread* targetThread = nullptr;
		std::uint64_t routeGeneration = 0;
		Core::FContentId targetContentId = Core::kInvalidContentId;
		Core::FContentInstanceId targetContentInstanceId = Core::kInvalidContentInstanceId;
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
				if (m_impl->config.enableTraceLogging)
				{
					std::ostringstream oss;
					oss << "runtime enqueue rejected. sessionId=" << sessionId
						<< " opcode=" << opcode
						<< " slotIndex=" << slotIndex
						<< " routeSessionId=" << route.sessionId
						<< " hasThread=" << (route.thread != nullptr ? 1 : 0);
					TraceRuntime(m_impl->config, sessionId, oss.str());
				}
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime enqueue failed: route mismatch or null thread.");
				}
				return false;
			}

			targetThread = route.thread;
			routeGeneration = route.routeGeneration;
			targetContentId = route.contentId;
			targetContentInstanceId = route.contentInstanceId;
		}

		Core::FOwnedPacketEnvelope packet{};
		packet.sessionId = sessionId;
		packet.routeGeneration = routeGeneration;
		packet.opcode = opcode;
		if (payload != nullptr && payloadLength > 0)
		{
			packet.payload.assign(payload, payload + payloadLength);
		}

		if (m_impl->config.enableTraceLogging)
		{
			std::ostringstream oss;
			oss << "runtime enqueue accepted. sessionId=" << sessionId
				<< " opcode=" << opcode
				<< " routeGeneration=" << routeGeneration
				<< " contentId=" << targetContentId
				<< " contentInstanceId=" << targetContentInstanceId
				<< " payloadBytes=" << payloadLength;
			TraceRuntime(m_impl->config, sessionId, oss.str());
		}

		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		targetThread->EnqueuePacket(std::move(packet));
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
		Threading::FContentThread* sourceThread = nullptr;
		Threading::FContentThread* targetThread = nullptr;
		Core::FContentId targetContentId = Core::kInvalidContentId;
		std::uint64_t sourceRouteGeneration = 0;
		std::uint64_t targetRouteGeneration = 0;
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

			auto targetIt = m_impl->contentSlots.find(targetContentInstanceId);
			if (targetIt == m_impl->contentSlots.end() || targetIt->second.thread == nullptr)
			{
				if (m_impl->config.failFastOnRuntimeError)
				{
					FailFastRuntime("ContentsRuntime move failed: target content thread missing.");
				}
				return false;
			}

			targetThread = targetIt->second.thread.get();
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

			if (route.thread != nullptr)
			{
				sourceThread = route.thread;
			}
			else
			{
				auto sourceIt = m_impl->contentSlots.find(route.contentInstanceId);
				if (sourceIt != m_impl->contentSlots.end() && sourceIt->second.thread != nullptr)
				{
					sourceThread = sourceIt->second.thread.get();
				}
			}

			sourceRouteGeneration = route.routeGeneration;
			targetRouteGeneration = sourceRouteGeneration + 1;
			route.contentId = targetContentId;
			route.contentInstanceId = targetContentInstanceId;
			route.routeGeneration = targetRouteGeneration;
			route.thread = targetThread;
		}

		m_impl->moveSessionCount.fetch_add(1, std::memory_order_relaxed);

		if (sourceThread != nullptr)
		{
			RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
			sourceThread->EnqueueLeave({ sessionId, sourceRouteGeneration });
		}
		RunRaceInjection(m_impl->config, m_impl->raceInjectionCounter);
		targetThread->EnqueueEnter({ sessionId, targetRouteGeneration, nullptr, std::move(onCompleted) });
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
