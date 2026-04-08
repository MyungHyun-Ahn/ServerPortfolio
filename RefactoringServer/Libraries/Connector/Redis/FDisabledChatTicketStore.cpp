#include "Pch.h"

#include "Connector/Redis/FDisabledChatTicketStore.h"

namespace Connector
{
	bool FDisabledChatTicketStore::TryConsumeChatTicket(
		std::string_view,
		SConsumedChatTicket& outTicket,
		std::string& outError)
	{
		outTicket = {};
		outError = "chat ticket store is disabled.";
		return false;
	}
}
