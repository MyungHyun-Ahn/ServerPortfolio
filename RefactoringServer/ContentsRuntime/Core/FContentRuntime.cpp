#include "Pch.h"

#include "ContentsRuntime/Core/FContentRuntime.h"

#include "ContentsRuntime/Core/FContentThread.h"
#include "ContentsRuntime/Core/IContent.h"
#include "Servers/IServer.h"

namespace ContentsRuntime::Core
{
	struct SContentSlot
	{
		std::unique_ptr<IContent> content;
		std::unique_ptr<FContentThread> thread;
	};

	struct FContentRuntime::SImpl
	{
		NetworkLib::IServer* server = nullptr;
		std::mutex lock;
		std::unordered_map<FContentId, SContentSlot> contentSlots;
		std::unordered_map<std::uint64_t, FContentId> sessionContentMap;
		std::atomic<std::uint64_t> moveSessionCount = 0;
		std::atomic<std::uint64_t> enqueueFailureCount = 0;
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

		std::lock_guard<std::mutex> lock(m_impl->lock);
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

	void FContentRuntime::Start(NetworkLib::IServer& server)
	{
		std::lock_guard<std::mutex> lock(m_impl->lock);
		if (m_impl->server != nullptr)
		{
			return;
		}

		m_impl->server = &server;
		for (auto& [contentId, slot] : m_impl->contentSlots)
		{
			(void)contentId;
			slot.thread = std::make_unique<FContentThread>(*slot.content, *this);
			slot.thread->Start();
		}
	}

	void FContentRuntime::Stop()
	{
		std::unordered_map<FContentId, std::unique_ptr<FContentThread>> threadsToStop;
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			for (auto& [contentId, slot] : m_impl->contentSlots)
			{
				(void)contentId;
				if (slot.thread != nullptr)
				{
					threadsToStop.emplace(contentId, std::move(slot.thread));
				}
			}
			m_impl->sessionContentMap.clear();
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
			std::lock_guard<std::mutex> lock(m_impl->lock);
			stats.registeredContentCount = static_cast<std::uint64_t>(m_impl->contentSlots.size());
			stats.activeSessionCount = static_cast<std::uint64_t>(m_impl->sessionContentMap.size());
			for (const auto& [sessionId, contentId] : m_impl->sessionContentMap)
			{
				(void)sessionId;
				++sessionCounts[contentId];
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

		stats.moveSessionCount = m_impl->moveSessionCount.load(std::memory_order_relaxed);
		stats.enqueueFailureCount = m_impl->enqueueFailureCount.load(std::memory_order_relaxed);
		return stats;
	}

	bool FContentRuntime::EnterSession(std::uint64_t sessionId, FContentId initialContentId)
	{
		FContentThread* targetThread = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			auto contentIt = m_impl->contentSlots.find(initialContentId);
			if (contentIt == m_impl->contentSlots.end() || contentIt->second.thread == nullptr)
			{
				return false;
			}

			m_impl->sessionContentMap[sessionId] = initialContentId;
			targetThread = contentIt->second.thread.get();
		}

		targetThread->EnqueueEnter(sessionId);
		return true;
	}

	void FContentRuntime::LeaveSession(std::uint64_t sessionId)
	{
		FContentThread* targetThread = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			auto sessionIt = m_impl->sessionContentMap.find(sessionId);
			if (sessionIt == m_impl->sessionContentMap.end())
			{
				return;
			}

			const FContentId currentContentId = sessionIt->second;
			m_impl->sessionContentMap.erase(sessionIt);

			auto contentIt = m_impl->contentSlots.find(currentContentId);
			if (contentIt != m_impl->contentSlots.end())
			{
				targetThread = contentIt->second.thread.get();
			}
		}

		if (targetThread != nullptr)
		{
			targetThread->EnqueueLeave(sessionId);
		}
	}

	bool FContentRuntime::EnqueuePacket(std::uint64_t sessionId, std::uint16_t opcode, const char* payload, std::int32_t payloadLength)
	{
		FContentThread* targetThread = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			auto sessionIt = m_impl->sessionContentMap.find(sessionId);
			if (sessionIt == m_impl->sessionContentMap.end())
			{
				m_impl->enqueueFailureCount.fetch_add(1, std::memory_order_relaxed);
				return false;
			}

			auto contentIt = m_impl->contentSlots.find(sessionIt->second);
			if (contentIt == m_impl->contentSlots.end() || contentIt->second.thread == nullptr)
			{
				m_impl->enqueueFailureCount.fetch_add(1, std::memory_order_relaxed);
				return false;
			}

			targetThread = contentIt->second.thread.get();
		}

		FOwnedPacketEnvelope packet{};
		packet.sessionId = sessionId;
		packet.opcode = opcode;
		if (payload != nullptr && payloadLength > 0)
		{
			packet.payload.assign(payload, payload + payloadLength);
		}

		targetThread->EnqueuePacket(std::move(packet));
		return true;
	}

	bool FContentRuntime::SendRaw(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length)
	{
		NetworkLib::IServer* server = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			server = m_impl->server;
		}

		return server != nullptr && server->Send(sessionId, opcode, buffer, length);
	}

	bool FContentRuntime::MoveSession(std::uint64_t sessionId, FContentId targetContentId)
	{
		FContentThread* sourceThread = nullptr;
		FContentThread* targetThread = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_impl->lock);
			auto targetIt = m_impl->contentSlots.find(targetContentId);
			if (targetIt == m_impl->contentSlots.end() || targetIt->second.thread == nullptr)
			{
				return false;
			}

			targetThread = targetIt->second.thread.get();

			const auto sessionIt = m_impl->sessionContentMap.find(sessionId);
			if (sessionIt != m_impl->sessionContentMap.end())
			{
				auto sourceIt = m_impl->contentSlots.find(sessionIt->second);
				if (sourceIt != m_impl->contentSlots.end())
				{
					sourceThread = sourceIt->second.thread.get();
				}
			}

			m_impl->sessionContentMap[sessionId] = targetContentId;
		}

		m_impl->moveSessionCount.fetch_add(1, std::memory_order_relaxed);

		if (sourceThread != nullptr)
		{
			sourceThread->EnqueueLeave(sessionId);
		}
		targetThread->EnqueueEnter(sessionId);
		return true;
	}
}
