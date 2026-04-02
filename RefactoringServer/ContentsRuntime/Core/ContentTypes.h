#pragma once

namespace ContentsRuntime::Core
{
	using FContentId = std::uint16_t;
	inline constexpr FContentId kInvalidContentId = 0;

	struct FOwnedPacketEnvelope
	{
		std::uint64_t sessionId = 0;
		std::uint16_t opcode = 0;
		std::vector<char> payload;
	};
}
