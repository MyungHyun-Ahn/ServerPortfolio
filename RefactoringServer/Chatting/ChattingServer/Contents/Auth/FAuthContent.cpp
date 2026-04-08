#include "Pch.h"

#include "ChattingServer/Contents/Auth/FAuthContent.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "Generated/Packets/Login/LoginPackets.h"

namespace ChattingServer::Contents
{
	FAuthContent::FAuthContent(
		std::shared_ptr<Foundation::ILogger> logger,
		const ContentsRuntime::Core::FContentInstanceId contentInstanceId,
		std::shared_ptr<FUserRegistry> userRegistry,
		std::shared_ptr<Connector::IChatTicketStore> chatTicketStore,
		SRuntimeOptions runtimeOptions)
		: m_logger(std::move(logger))
		, m_contentInstanceId(contentInstanceId)
		, m_userRegistry(std::move(userRegistry))
		, m_chatTicketStore(std::move(chatTicketStore))
		, m_runtimeOptions(std::move(runtimeOptions))
	{
	}

	ContentsRuntime::Core::FContentId FAuthContent::GetContentId() const noexcept
	{
		return kAuthContentId;
	}

	ContentsRuntime::Core::FContentInstanceId FAuthContent::GetContentInstanceId() const noexcept
	{
		return m_contentInstanceId;
	}

	void FAuthContent::OnEnter(std::uint64_t sessionId, std::uint64_t routeGeneration, ContentsRuntime::Bridge::IContentBridge&)
	{
		m_sessionGenerations[sessionId] = routeGeneration;
		std::ostringstream oss;
		oss << "auth content enter. sessionId=" << sessionId
			<< " routeGeneration=" << routeGeneration;
		Log(Foundation::ELogLevel::Info, oss.str());
	}

	void FAuthContent::OnLeave(std::uint64_t sessionId, std::uint64_t routeGeneration, ContentsRuntime::Bridge::IContentBridge&)
	{
		const auto generationIt = m_sessionGenerations.find(sessionId);
		const bool isCurrentGeneration =
			generationIt != m_sessionGenerations.end() &&
			generationIt->second == routeGeneration;
		std::ostringstream oss;
		oss << "auth content leave. sessionId=" << sessionId
			<< " routeGeneration=" << routeGeneration
			<< " stale=" << (isCurrentGeneration ? 0 : 1);
		Log(Foundation::ELogLevel::Info, oss.str());
		if (isCurrentGeneration)
		{
			m_sessionGenerations.erase(sessionId);
		}
	}

	void FAuthContent::OnPacket(
		const std::uint64_t sessionId,
		const std::uint64_t routeGeneration,
		const std::uint16_t opcode,
		std::span<const char> payload,
		ContentsRuntime::Bridge::IContentBridge& bridge)
	{
		const auto currentContentInstanceId = bridge.GetCurrentContentInstanceId(sessionId);
		if (!currentContentInstanceId.has_value() || *currentContentInstanceId != m_contentInstanceId)
		{
			return;
		}

		const auto generationIt = m_sessionGenerations.find(sessionId);
		if (generationIt == m_sessionGenerations.end() || generationIt->second != routeGeneration)
		{
			return;
		}

		if (opcode == Generated::Login::FLoginRq::kOpcode)
		{
			Generated::Login::FLoginRq requestPacket;
			if (!ContentsRuntime::Bridge::DeserializeOwnedPacket(opcode, payload, requestPacket))
			{
				Log(Foundation::ELogLevel::Warn, "login deserialize failed.");
				return;
			}

			bool success = requestPacket.userId != 0;
			if (success && m_userRegistry != nullptr)
			{
				m_userRegistry->UpsertUser(sessionId, requestPacket.userId);
			}

			if (success && !bridge.MoveSession(sessionId, kLobbyContentId))
			{
				std::ostringstream oss;
				oss << "move to lobby content failed. sessionId=" << sessionId
					<< " userId=" << requestPacket.userId;
				Log(Foundation::ELogLevel::Error, oss.str());
				success = false;
				if (m_userRegistry != nullptr)
				{
					m_userRegistry->RemoveUser(sessionId);
				}
			}

			Generated::Login::FLoginRp responsePacket;
			responsePacket.userId = requestPacket.userId;
			responsePacket.success = success;
			if (!ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket))
			{
				if (success && m_userRegistry != nullptr)
				{
					m_userRegistry->RemoveUser(sessionId);
				}

				std::ostringstream oss;
				oss << "login response send failed. sessionId=" << sessionId
					<< " userId=" << requestPacket.userId;
				Log(Foundation::ELogLevel::Error, oss.str());
				return;
			}

			if (!success)
			{
				if (m_userRegistry != nullptr)
				{
					m_userRegistry->RemoveUser(sessionId);
				}

				std::ostringstream oss;
				oss << "login rejected. sessionId=" << sessionId
					<< " userId=" << requestPacket.userId;
				Log(Foundation::ELogLevel::Warn, oss.str());
				return;
			}

			if (m_runtimeOptions.bootstrapTrace &&
				m_runtimeOptions.traceUserId != 0 &&
				requestPacket.userId == m_runtimeOptions.traceUserId &&
				m_runtimeOptions.tracedSessionId != nullptr)
			{
				m_runtimeOptions.tracedSessionId->store(sessionId, std::memory_order_relaxed);
				std::ostringstream oss;
				oss << "bootstrap trace target mapped. userId=" << requestPacket.userId
					<< " sessionId=" << sessionId;
				Log(Foundation::ELogLevel::Info, oss.str());
			}

			std::ostringstream oss;
			oss << "legacy login succeeded. sessionId=" << sessionId
				<< " userId=" << requestPacket.userId;
			Log(Foundation::ELogLevel::Info, oss.str());
			return;
		}

		if (opcode != Generated::Login::FLoginAuthRq::kOpcode)
		{
			return;
		}

		Generated::Login::FLoginAuthRq requestPacket;
		if (!ContentsRuntime::Bridge::DeserializeOwnedPacket(opcode, payload, requestPacket))
		{
			Log(Foundation::ELogLevel::Warn, "login auth deserialize failed.");
			return;
		}

		Generated::Login::FLoginAuthRp responsePacket;
		Connector::SConsumedChatTicket consumedTicket{};
		std::string authError;
		bool success = false;
		if (m_chatTicketStore == nullptr)
		{
			authError = "chat ticket store is not configured.";
		}
		else
		{
			success = m_chatTicketStore->TryConsumeChatTicket(requestPacket.ticket, consumedTicket, authError);
		}

		if (success && consumedTicket.valid && consumedTicket.userId != 0 && m_userRegistry != nullptr)
		{
			m_userRegistry->UpsertUser(sessionId, consumedTicket.userId);
		}

		if (success && (!consumedTicket.valid || consumedTicket.userId == 0))
		{
			success = false;
			authError = "chat ticket payload is invalid.";
		}

		if (success && !bridge.MoveSession(sessionId, kLobbyContentId))
		{
			success = false;
			authError = "move to lobby content failed.";
			if (m_userRegistry != nullptr)
			{
				m_userRegistry->RemoveUser(sessionId);
			}
		}

		responsePacket.userId = success ? consumedTicket.userId : 0;
		responsePacket.success = success;
		if (!ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket))
		{
			if (success && m_userRegistry != nullptr)
			{
				m_userRegistry->RemoveUser(sessionId);
			}

			std::ostringstream oss;
			oss << "login auth response send failed. sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Error, oss.str());
			return;
		}

		if (!success)
		{
			if (m_userRegistry != nullptr)
			{
				m_userRegistry->RemoveUser(sessionId);
			}

			std::ostringstream oss;
			oss << "login auth rejected. sessionId=" << sessionId
				<< " reason=" << authError;
			Log(Foundation::ELogLevel::Warn, oss.str());
			return;
		}

		if (m_runtimeOptions.bootstrapTrace &&
			m_runtimeOptions.traceUserId != 0 &&
			consumedTicket.userId == m_runtimeOptions.traceUserId &&
			m_runtimeOptions.tracedSessionId != nullptr)
		{
			m_runtimeOptions.tracedSessionId->store(sessionId, std::memory_order_relaxed);
			std::ostringstream oss;
			oss << "bootstrap trace target mapped. userId=" << consumedTicket.userId
				<< " sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		std::ostringstream oss;
		oss << "login auth succeeded. sessionId=" << sessionId
			<< " userId=" << consumedTicket.userId;
		Log(Foundation::ELogLevel::Info, oss.str());
	}

	void FAuthContent::Log(Foundation::ELogLevel logLevel, const std::string& message) const
	{
		if (m_logger != nullptr)
		{
			m_logger->Log(logLevel, "ChattingServer", message);
		}
	}
}
