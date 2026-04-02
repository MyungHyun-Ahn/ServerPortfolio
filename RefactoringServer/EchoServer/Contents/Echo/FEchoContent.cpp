#include "Pch.h"

#include "EchoServer/Contents/Echo/FEchoContent.h"

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "EchoServer/Contents/ContentTypes.h"
#include "Generated/Packets/Chat/ChatPackets.h"
#include "Generated/Packets/Echo/EchoPackets.h"

namespace EchoServer::Contents
{
	FEchoContent::FEchoContent(std::shared_ptr<Foundation::ILogger> logger, SRuntimeOptions runtimeOptions)
		: m_logger(std::move(logger))
		, m_runtimeOptions(runtimeOptions)
	{
	}

	ContentsRuntime::Core::FContentId FEchoContent::GetContentId() const noexcept
	{
		return kEchoContentId;
	}

	void FEchoContent::OnEnter(std::uint64_t sessionId, ContentsRuntime::Bridge::IContentBridge&)
	{
		std::ostringstream oss;
		oss << "echo content enter. sessionId=" << sessionId;
		Log(Foundation::ELogLevel::Info, oss.str());
	}

	void FEchoContent::OnLeave(std::uint64_t sessionId, ContentsRuntime::Bridge::IContentBridge&)
	{
		std::ostringstream oss;
		oss << "echo content leave. sessionId=" << sessionId;
		Log(Foundation::ELogLevel::Info, oss.str());
	}

	void FEchoContent::OnPacket(
		std::uint64_t sessionId,
		std::uint16_t opcode,
		std::span<const char> payload,
		ContentsRuntime::Bridge::IContentBridge& bridge)
	{
		switch (opcode)
		{
		case Generated::Echo::FEchoRq::kOpcode:
			HandleEchoRq(sessionId, payload, bridge);
			return;
		case Generated::Chat::FRoomSnapshotRq::kOpcode:
			HandleRoomSnapshotRq(sessionId, payload, bridge);
			return;
		default:
			return;
		}
	}

	void FEchoContent::HandleEchoRq(
		std::uint64_t sessionId,
		std::span<const char> payload,
		ContentsRuntime::Bridge::IContentBridge& bridge)
	{
		Generated::Echo::FEchoRq packet;
		NetworkLib::Packet::View::FBorrowedViewScope borrowedViewScope;
		if (!ContentsRuntime::Bridge::DeserializeOwnedPacket(Generated::Echo::FEchoRq::kOpcode, payload, borrowedViewScope, packet))
		{
			Log(Foundation::ELogLevel::Warn, "echo deserialize failed.");
			return;
		}

		if (m_runtimeOptions.logPackets)
		{
			std::ostringstream oss;
			oss << "received. sessionId=" << sessionId
				<< " opcode=" << packet.GetOpcode()
				<< " message=" << packet.GetMessageValue();
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		if (m_runtimeOptions.sendThreadCount == 1 && m_runtimeOptions.responsesPerThread == 1)
		{
			Generated::Echo::FEchoRp responsePacket;
			responsePacket.SetMessageValue(packet.GetMessageValue());
			ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket);
			return;
		}

		std::vector<std::thread> sendThreads;
		sendThreads.reserve(static_cast<std::size_t>(m_runtimeOptions.sendThreadCount));
		for (int threadIndex = 0; threadIndex < m_runtimeOptions.sendThreadCount; ++threadIndex)
		{
			sendThreads.emplace_back([&, threadIndex, sessionId, message = std::string(packet.GetMessageValue())]()
			{
				for (int responseIndex = 0; responseIndex < m_runtimeOptions.responsesPerThread; ++responseIndex)
				{
					std::ostringstream responseBuilder;
					responseBuilder << message
						<< "|t=" << threadIndex
						<< "|r=" << responseIndex;

					Generated::Echo::FEchoRp responsePacket;
					responsePacket.SetMessageValue(responseBuilder.str());
					ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket);
				}
			});
		}

		for (std::thread& sendThread : sendThreads)
		{
			sendThread.join();
		}
	}

	void FEchoContent::HandleRoomSnapshotRq(
		std::uint64_t sessionId,
		std::span<const char> payload,
		ContentsRuntime::Bridge::IContentBridge& bridge)
	{
		if (m_runtimeOptions.bootstrapTrace)
		{
			std::ostringstream oss;
			oss << "bootstrap trace: snapshot request reached echo content. sessionId=" << sessionId
				<< " payloadBytes=" << payload.size();
			Log(Foundation::ELogLevel::Info, oss.str());
		}

		Generated::Chat::FRoomSnapshotRq packet;
		if (!ContentsRuntime::Bridge::DeserializeOwnedPacket(Generated::Chat::FRoomSnapshotRq::kOpcode, payload, packet))
		{
			Log(Foundation::ELogLevel::Warn, "chat snapshot deserialize failed.");
			return;
		}

		Generated::Chat::FRoomSnapshotRp snapshotPacket;
		snapshotPacket.roomId = packet.roomId;
		snapshotPacket.participants = { "alpha", "bravo", "charlie" };
		snapshotPacket.unreadCounts = {
			{ "alpha", 1u },
			{ "bravo", 3u },
			{ "charlie", 5u }
		};
		snapshotPacket.metadata = {
			{ "topic", "general" },
			{ "owner", "alpha" }
		};
		if (!ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, snapshotPacket))
		{
			std::ostringstream oss;
			oss << "chat snapshot response send failed. sessionId=" << sessionId
				<< " roomId=" << packet.roomId;
			Log(Foundation::ELogLevel::Error, oss.str());
			return;
		}

		const std::array<std::uint8_t, 8> binaryPayload = {
			static_cast<std::uint8_t>(packet.roomId & 0xFF),
			static_cast<std::uint8_t>((packet.roomId >> 8) & 0xFF),
			0x10, 0x20, 0x30, 0x40, 0x50, 0x60
		};

		Generated::Chat::FRoomBinarySnapshotNoti binarySnapshotPacket;
		binarySnapshotPacket.roomId = packet.roomId;
		binarySnapshotPacket.SetPayloadValue(std::span<const std::uint8_t>(binaryPayload.data(), binaryPayload.size()));
		if (!ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, binarySnapshotPacket))
		{
			std::ostringstream oss;
			oss << "chat binary snapshot send failed. sessionId=" << sessionId
				<< " roomId=" << packet.roomId;
			Log(Foundation::ELogLevel::Error, oss.str());
			return;
		}

		if (m_runtimeOptions.logPackets)
		{
			std::ostringstream oss;
			oss << "chat snapshot served. sessionId=" << sessionId
				<< " roomId=" << packet.roomId
				<< " binaryBytes=" << binaryPayload.size();
			Log(Foundation::ELogLevel::Info, oss.str());
		}
		else if (m_runtimeOptions.bootstrapTrace)
		{
			std::ostringstream oss;
			oss << "bootstrap trace: snapshot responses sent. sessionId=" << sessionId
				<< " roomId=" << packet.roomId;
			Log(Foundation::ELogLevel::Info, oss.str());
		}
	}

	void FEchoContent::Log(Foundation::ELogLevel logLevel, const std::string& message) const
	{
		if (m_logger != nullptr)
		{
			m_logger->Log(logLevel, "EchoServer", message);
		}
	}
}
