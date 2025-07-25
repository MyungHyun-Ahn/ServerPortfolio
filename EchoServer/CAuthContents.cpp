#include "pch.h"
#include "CPlayer.h"
#include "CAuthContents.h"
#include "EchoProtocol.h"
#include "CEchoContents.h"
#include "CEchoServer.h"
#include "CGenPacket.h"

namespace EchoServer::Contents
{
	void CAuthContents::OnEnter(const UINT64 sessionID, void *pObject) noexcept
	{
		EchoServer::Objects::CNonLoginPlayer *pNonLoginPlayer = EchoServer::Objects::CNonLoginPlayer::Alloc();
		m_umapSessions.insert(std::make_pair(sessionID, pNonLoginPlayer));
	}

	void CAuthContents::OnLeave(const UINT64 sessionID) noexcept
	{
		auto it = m_umapSessions.find(sessionID);
		if (it == m_umapSessions.end())
		{
			// __debugbreak();
			return;
		}

		EchoServer::Objects::CNonLoginPlayer *pNonLoginPlayer = (EchoServer::Objects::CNonLoginPlayer *)it->second;
		EchoServer::Objects::CNonLoginPlayer::Free(pNonLoginPlayer);
		m_umapSessions.erase(sessionID);
	}

	NetworkLib::Contents::RECV_RET CAuthContents::OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept
	{
		WORD wordType;
		*message >> wordType;

		EchoServer::Protocol::PACKET_TYPE type = static_cast<EchoServer::Protocol::PACKET_TYPE>(wordType);

		switch (type)
		{
		case EchoServer::Protocol::PACKET_TYPE::CS_GAME_REQ_LOGIN:
		{
			EchoServer::Objects::CNonLoginPlayer *pNonLoginPlayer = static_cast<EchoServer::Objects::CNonLoginPlayer *>(m_umapSessions[sessionID]);

			INT64 accountNo;
			char sessionKey[64];
			int version;

			if (message->GetDataSize() != sizeof(INT64) + 64 * sizeof(char) + sizeof(int))
				return  NetworkLib::Contents::RECV_RET::RECV_FALSE;

			*message >> accountNo;
			message->Dequeue(sessionKey, sizeof(char) * 64);
			*message >> version;

			if (message->GetDataSize() != 0)
				return  NetworkLib::Contents::RECV_RET::RECV_FALSE;

			// 버전 검사, 세션 키 인증
			// 인증 통과
			
			// Auth Content에서 삭제
			LeaveJobEnqueue(sessionID);
			EchoServer::Contents::CEchoContents *pEchoContents 
				= static_cast<EchoServer::Server::CEchoServer *>(NetworkLib::Core::Net::Server::g_NetServer)->m_pEchoContents;

			EchoServer::Objects::CPlayer *pPlayer = EchoServer::Objects::CPlayer::Alloc();
			pPlayer->m_iAccountNo = accountNo;
			pEchoContents->MoveJobEnqueue(sessionID, pPlayer);

			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pLoginRes
				= EchoServer::Protocol::CGenPacket::makePacketResLogin(TRUE, accountNo);
			NetworkLib::Core::Net::Server::g_NetServer->SendPacket(sessionID, pLoginRes);
		}
			break;
		default:
			break;
		}

		return NetworkLib::Contents::RECV_RET::RECV_FALSE;
	}

	void CAuthContents::OnLoopEnd() noexcept
	{
	}
}