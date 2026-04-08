#include "Pch.h"

#include "EchoServer/Contents/Auth/FAuthContent.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "EchoServer/Contents/ContentTypes.h"
#include "Generated/Packets/Chat/ChatPackets.h"
#include "Generated/Packets/Login/LoginPackets.h"

namespace EchoServer::Contents
{
	FAuthContent::FAuthContent(
		std::shared_ptr<Foundation::ILogger> logger,
		const ContentsRuntime::Core::FContentInstanceId contentInstanceId,
		SRuntimeOptions runtimeOptions)
		: m_logger(std::move(logger))
		, m_contentInstanceId(contentInstanceId)
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
		std::uint64_t sessionId,
		std::uint64_t routeGeneration,
		std::uint16_t opcode,
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

		if (opcode != Generated::Login::FLoginRq::kOpcode)
			return;

		Generated::Login::FLoginRq packet;
		if (!ContentsRuntime::Bridge::DeserializeOwnedPacket(opcode, payload, packet))
		{
			Log(Foundation::ELogLevel::Warn, "login deserialize failed.");
			return;
		}

		const bool success = packet.userId != 0;
		{
			std::ostringstream oss;
			oss << "login " << (success ? "succeeded" : "failed")
				<< ". sessionId=" << sessionId
				<< " userId=" << packet.userId;
			Log(success ? Foundation::ELogLevel::Info : Foundation::ELogLevel::Warn, oss.str());
		}

		if (success &&
			m_runtimeOptions.bootstrapTrace &&
			m_runtimeOptions.traceUserId != 0 &&
			packet.userId == m_runtimeOptions.traceUserId &&
			m_runtimeOptions.tracedSessionId != nullptr)
		{
			m_runtimeOptions.tracedSessionId->store(sessionId, std::memory_order_relaxed);
			std::ostringstream oss;
			oss << "bootstrap trace target mapped. userId=" << packet.userId
				<< " sessionId=" << sessionId;
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		Generated::Login::FLoginRp responsePacket;
		responsePacket.userId = packet.userId;
		responsePacket.success = success;
		if (success)
		{
			const bool moveSucceeded = bridge.MoveSession(sessionId, kLobbyContentId);
			if (!moveSucceeded)
			{
				std::ostringstream oss;
				oss << "move to lobby content failed. sessionId=" << sessionId
					<< " userId=" << packet.userId;
				Log(Foundation::ELogLevel::Error, oss.str());
				return;
			}
		}

		if (!ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket))
		{
			std::ostringstream oss;
			oss << "login response send failed. sessionId=" << sessionId
				<< " userId=" << packet.userId;
			Log(Foundation::ELogLevel::Error, oss.str());
			return;
		}
	}

	void FAuthContent::Log(Foundation::ELogLevel logLevel, const std::string& message) const
	{
		if (m_logger != nullptr)
		{
			m_logger->Log(logLevel, "EchoServer", message);
		}
	}
}
