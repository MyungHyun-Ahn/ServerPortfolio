#pragma once

namespace ContentsRuntime::Core
{
	using FContentId = std::uint16_t;
	using FContentInstanceId = std::uint32_t;
	using FTransitionCompletionCallback = std::function<void()>;
}

namespace ContentsRuntime::Bridge
{
	class IContentBridge
	{
	public:
		virtual ~IContentBridge() = default;

		virtual bool SendRaw(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) = 0;
		virtual bool MoveSession(std::uint64_t sessionId, Core::FContentId targetContentId) = 0;
		virtual bool MoveSessionToInstance(std::uint64_t sessionId, Core::FContentInstanceId targetContentInstanceId) = 0;
		virtual bool MoveSessionWithCompletion(
			std::uint64_t sessionId,
			Core::FContentId targetContentId,
			Core::FTransitionCompletionCallback onCompleted) = 0;
		virtual bool MoveSessionToInstanceWithCompletion(
			std::uint64_t sessionId,
			Core::FContentInstanceId targetContentInstanceId,
			Core::FTransitionCompletionCallback onCompleted) = 0;
		virtual bool DisconnectSession(std::uint64_t sessionId) = 0;
		virtual bool IsSessionAlive(std::uint64_t sessionId) const = 0;
		virtual bool HasContentInstance(Core::FContentInstanceId contentInstanceId) const = 0;
		virtual std::optional<Core::FContentId> GetCurrentContentId(std::uint64_t sessionId) const = 0;
		virtual std::optional<Core::FContentInstanceId> GetCurrentContentInstanceId(std::uint64_t sessionId) const = 0;
	};

	template <typename TPacket>
	inline bool SendContentPacket(IContentBridge& bridge, std::uint64_t sessionId, const TPacket& packet)
	{
		NetworkLib::Packet::Serialization::FPacketWriter writer;
		writer.ReserveAdditional(packet.GetEstimatedBodySize());
		packet.Serialize(writer);
		const std::vector<char>& payload = writer.GetBuffer();
		return bridge.SendRaw(
			sessionId,
			packet.GetOpcode(),
			payload.empty() ? nullptr : payload.data(),
			static_cast<std::int32_t>(payload.size()));
	}

	template <typename TPacket>
	inline bool DeserializeOwnedPacket(std::uint16_t opcode, std::span<const char> payload, TPacket& outPacket)
	{
		NetworkLib::Packet::View::FPacketView packetView{};
		packetView.opcode = opcode;
		packetView.payload = payload.empty() ? nullptr : payload.data();
		packetView.payloadLength = static_cast<std::int32_t>(payload.size());
		return NetworkLib::Packet::Serialization::DeserializeContentPacket(packetView, outPacket);
	}

	template <typename TPacket>
	inline bool DeserializeOwnedPacket(
		std::uint16_t opcode,
		std::span<const char> payload,
		NetworkLib::Packet::View::FBorrowedViewScope& borrowedViewScope,
		TPacket& outPacket)
	{
		outPacket.BindBorrowedViewScope(borrowedViewScope.GetState());
		return DeserializeOwnedPacket(opcode, payload, outPacket);
	}
}
