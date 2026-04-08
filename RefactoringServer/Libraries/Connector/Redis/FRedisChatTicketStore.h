#pragma once

#include "Connector/Config/RedisChatTicketStoreTypes.h"
#include "Connector/Interfaces/IChatTicketStore.h"

#include <mutex>

namespace cpp_redis
{
	class client;
}

namespace Connector
{
	class FRedisChatTicketStore final : public IChatTicketStore
	{
	public:
		explicit FRedisChatTicketStore(SRedisChatTicketStoreConfig config);
		~FRedisChatTicketStore() override;

		bool TryConsumeChatTicket(
			std::string_view ticket,
			SConsumedChatTicket& outTicket,
			std::string& outError) override;

	private:
		bool EnsureConnected(std::string& outError);
		std::string BuildTicketKey(std::string_view ticket) const;

	private:
		mutable std::mutex m_mutex;
		SRedisChatTicketStoreConfig m_config;
		std::unique_ptr<cpp_redis::client> m_client;
		bool m_authenticated = false;
		bool m_selectedDatabase = false;
	};
}
