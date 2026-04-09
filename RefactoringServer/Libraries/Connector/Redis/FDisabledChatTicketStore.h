#pragma once

#include "Connector/Interfaces/IChatTicketStore.h"

namespace Connector
{
	class FDisabledChatTicketStore final : public IChatTicketStore
	{
	public:
		bool TryConsumeChatTicket(
			std::string_view ticket,
			SConsumedChatTicket& outTicket,
			std::string& outError) override;
	};
}
