#include "Pch.h"

#include "ChattingServer/Contents/Session/FUserRegistry.h"

namespace ChattingServer::Contents
{
	void FUserRegistry::UpsertUser(const std::uint64_t sessionId, const std::uint32_t userId)
	{
		std::lock_guard<std::mutex> lock(m_lock);
		m_sessionUsers[sessionId] = userId;
	}

	void FUserRegistry::RemoveUser(const std::uint64_t sessionId)
	{
		std::lock_guard<std::mutex> lock(m_lock);
		m_sessionUsers.erase(sessionId);
	}

	std::optional<std::uint32_t> FUserRegistry::GetUserId(const std::uint64_t sessionId) const
	{
		std::lock_guard<std::mutex> lock(m_lock);
		const auto it = m_sessionUsers.find(sessionId);
		if (it == m_sessionUsers.end())
		{
			return std::nullopt;
		}

		return it->second;
	}
}
