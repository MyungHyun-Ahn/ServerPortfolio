#include "Pch.h"

#include "EchoServer/Contents/Auth/FAuthContent.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "EchoServer/Contents/ContentTypes.h"
#include "Generated/Packets/Login/LoginPackets.h"

namespace EchoServer::Contents
{
	FAuthContent::FAuthContent(std::shared_ptr<Foundation::ILogger> logger)
		: m_logger(std::move(logger))
	{
	}

	ContentsRuntime::Core::FContentId FAuthContent::GetContentId() const noexcept
	{
		return kAuthContentId;
	}

	void FAuthContent::OnEnter(std::uint64_t sessionId, ContentsRuntime::Bridge::IContentBridge&)
	{
		std::ostringstream oss;
		oss << "auth content enter. sessionId=" << sessionId;
		Log(Foundation::ELogLevel::Info, oss.str());
	}

	void FAuthContent::OnLeave(std::uint64_t sessionId, ContentsRuntime::Bridge::IContentBridge&)
	{
		std::ostringstream oss;
		oss << "auth content leave. sessionId=" << sessionId;
		Log(Foundation::ELogLevel::Info, oss.str());
	}

	void FAuthContent::OnPacket(
		std::uint64_t sessionId,
		std::uint16_t opcode,
		std::span<const char> payload,
		ContentsRuntime::Bridge::IContentBridge& bridge)
	{
		if (opcode != Generated::Login::FLoginRq::kOpcode)
		{
			return;
		}

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

		Generated::Login::FLoginRp responsePacket;
		responsePacket.userId = packet.userId;
		responsePacket.success = success;
		ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket);

		if (success)
		{
			bridge.MoveSession(sessionId, kEchoContentId);
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
