#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Connector
{
	struct SConsumedChatTicket
	{
		std::uint32_t userId = 0;
		std::uint64_t loginVersion = 0;
		bool valid = false;
	};

	class IChatTicketStore
	{
	public:
		virtual ~IChatTicketStore() = default;

		virtual bool TryConsumeChatTicket(
			std::string_view ticket,
			SConsumedChatTicket& outTicket,
			std::string& outError) = 0;
	};
}
