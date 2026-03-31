#include "pch.h"
#include "CEntranceContents.h"
#include "CGenPacket.h"
#include "CPlayerSessionContext.h"
#include "WorldProtocol.h"
#include "NetworkLib/CNetServer.h"

namespace WorldServer::Contents
{
	void CEntranceContents::OnEnter(const UINT64 sessionID, void *pObject) noexcept
	{
		WorldServer::Objects::CPlayerSessionContext *pContext = static_cast<WorldServer::Objects::CPlayerSessionContext *>(pObject);
		if (pContext == nullptr)
		{
			pContext = WorldServer::Objects::CPlayerSessionContext::Alloc();
		}

		pContext->m_dwPrevRecvTime = GetTickCount();
		m_umapSessions.insert(std::make_pair(sessionID, pContext));
	}

	void CEntranceContents::OnLeave(const UINT64 sessionID) noexcept
	{
		auto it = m_umapSessions.find(sessionID);
		if (it == m_umapSessions.end())
		{
			return;
		}

		WorldServer::Objects::CPlayerSessionContext *pContext = static_cast<WorldServer::Objects::CPlayerSessionContext *>(it->second);
		WorldServer::Objects::CPlayerSessionContext::Free(pContext);
		m_umapSessions.erase(sessionID);
	}

	NetworkLib::Contents::RECV_RET CEntranceContents::OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept
	{
		UNREFERENCED_PARAMETER(delayFrame);

		auto it = m_umapSessions.find(sessionID);
		if (it == m_umapSessions.end())
		{
			return NetworkLib::Contents::RECV_RET::RECV_FALSE;
		}

		WorldServer::Objects::CPlayerSessionContext *pContext = static_cast<WorldServer::Objects::CPlayerSessionContext *>(it->second);
		WORD wordType;
		*message >> wordType;

		WorldServer::Protocol::PACKET_TYPE type = static_cast<WorldServer::Protocol::PACKET_TYPE>(wordType);
		switch (type)
		{
		case WorldServer::Protocol::PACKET_TYPE::CS_GAME_REQ_LOGIN:
		{
			INT64 accountNo;
			char sessionKey[64];
			int version;

			if (message->GetDataSize() != sizeof(INT64) + sizeof(sessionKey) + sizeof(int))
			{
				return NetworkLib::Contents::RECV_RET::RECV_FALSE;
			}

			*message >> accountNo;
			message->Dequeue(sessionKey, sizeof(sessionKey));
			*message >> version;

			UNREFERENCED_PARAMETER(sessionKey);
			UNREFERENCED_PARAMETER(version);

			if (message->GetDataSize() != 0)
			{
				return NetworkLib::Contents::RECV_RET::RECV_FALSE;
			}

			pContext->m_iAccountNo = accountNo;
			pContext->m_isLoggedIn = true;
			pContext->m_dwPrevRecvTime = GetTickCount();

			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pLoginRes
				= WorldServer::Protocol::CGenPacket::MakePacketResLogin(TRUE, accountNo);
			NetworkLib::Core::Net::Server::g_NetServer->SendPacket(sessionID, pLoginRes);
			return NetworkLib::Contents::RECV_RET::RECV_TRUE;
		}
		case WorldServer::Protocol::PACKET_TYPE::CS_GAME_REQ_ECHO:
		{
			INT64 accountNo;
			LONGLONG sendTick;

			if (!pContext->m_isLoggedIn)
			{
				return NetworkLib::Contents::RECV_RET::RECV_FALSE;
			}
			if (message->GetDataSize() != sizeof(INT64) + sizeof(LONGLONG))
			{
				return NetworkLib::Contents::RECV_RET::RECV_FALSE;
			}

			*message >> accountNo;
			*message >> sendTick;

			if (message->GetDataSize() != 0)
			{
				return NetworkLib::Contents::RECV_RET::RECV_FALSE;
			}

			pContext->m_dwPrevRecvTime = GetTickCount();

			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pEchoRes
				= WorldServer::Protocol::CGenPacket::MakePacketResEcho(accountNo, sendTick);
			NetworkLib::Core::Net::Server::g_NetServer->SendPacket(sessionID, pEchoRes);
			return NetworkLib::Contents::RECV_RET::RECV_TRUE;
		}
		case WorldServer::Protocol::PACKET_TYPE::CS_GAME_REQ_HEARTBEAT:
		{
			if (message->GetDataSize() != 0)
			{
				return NetworkLib::Contents::RECV_RET::RECV_FALSE;
			}

			pContext->m_dwPrevRecvTime = GetTickCount();
			return NetworkLib::Contents::RECV_RET::RECV_TRUE;
		}
		default:
			return NetworkLib::Contents::RECV_RET::RECV_FALSE;
		}
	}

	void CEntranceContents::OnLoopEnd() noexcept
	{
	}
}
