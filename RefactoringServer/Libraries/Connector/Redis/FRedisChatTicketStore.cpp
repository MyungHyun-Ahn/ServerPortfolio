#include "Pch.h"

#include "Connector/Redis/FRedisChatTicketStore.h"

#include <cpp_redis/core/client.hpp>

namespace Connector
{
	namespace
	{
		bool TryParseUserId(std::string_view value, std::uint32_t& outUserId) noexcept
		{
			outUserId = 0;
			if (value.empty())
			{
				return false;
			}

			const char* const begin = value.data();
			const char* const end = value.data() + value.size();
			const auto [parseEnd, parseError] = std::from_chars(begin, end, outUserId);
			return parseError == std::errc() && parseEnd == end && outUserId != 0;
		}
	}

	FRedisChatTicketStore::FRedisChatTicketStore(SRedisChatTicketStoreConfig config)
		: m_config(std::move(config))
	{
	}

	FRedisChatTicketStore::~FRedisChatTicketStore()
	{
		std::scoped_lock lock(m_mutex);
		if (m_client != nullptr && m_client->is_connected())
		{
			m_client->disconnect(true);
		}
	}

	bool FRedisChatTicketStore::TryConsumeChatTicket(
		const std::string_view ticket,
		SConsumedChatTicket& outTicket,
		std::string& outError)
	{
		std::scoped_lock lock(m_mutex);
		outTicket = {};
		outError.clear();

		if (ticket.empty())
		{
			outError = "chat ticket is empty.";
			return false;
		}

		if (!EnsureConnected(outError))
		{
			return false;
		}

		try
		{
			auto replyFuture = m_client->send({ "GETDEL", BuildTicketKey(ticket) });
			m_client->sync_commit();
			cpp_redis::reply reply = replyFuture.get();
			if (reply.is_null())
			{
				outError = "chat ticket not found.";
				return false;
			}

			if (reply.is_error())
			{
				outError = reply.error();
				return false;
			}

			if (!reply.is_string())
			{
				outError = "chat ticket reply type is invalid.";
				return false;
			}

			if (!TryParseUserId(reply.as_string(), outTicket.userId))
			{
				outError = "chat ticket payload is invalid.";
				return false;
			}

			outTicket.valid = true;
			return true;
		}
		catch (const std::exception& exception)
		{
			outError = exception.what();
			return false;
		}
	}

	bool FRedisChatTicketStore::EnsureConnected(std::string& outError)
	{
		outError.clear();

		try
		{
			if (m_client == nullptr)
			{
				m_client = std::make_unique<cpp_redis::client>();
			}

			if (!m_client->is_connected())
			{
				m_client->connect(
					m_config.connection.host,
					m_config.connection.port,
					nullptr,
					m_config.connection.connectTimeoutMs);
				m_authenticated = false;
				m_selectedDatabase = false;
			}

			if (!m_authenticated && !m_config.connection.password.empty())
			{
				auto authReplyFuture = m_client->auth(m_config.connection.password);
				m_client->sync_commit();
				cpp_redis::reply authReply = authReplyFuture.get();
				if (authReply.is_error())
				{
					outError = authReply.error();
					return false;
				}

				m_authenticated = true;
			}

			if (!m_selectedDatabase && m_config.connection.database != 0)
			{
				auto selectReplyFuture = m_client->select(m_config.connection.database);
				m_client->sync_commit();
				cpp_redis::reply selectReply = selectReplyFuture.get();
				if (selectReply.is_error())
				{
					outError = selectReply.error();
					return false;
				}

				m_selectedDatabase = true;
			}

			return true;
		}
		catch (const std::exception& exception)
		{
			outError = exception.what();
			return false;
		}
	}

	std::string FRedisChatTicketStore::BuildTicketKey(const std::string_view ticket) const
	{
		std::string key = m_config.keyPrefix;
		key.append(ticket.data(), ticket.size());
		return key;
	}
}
