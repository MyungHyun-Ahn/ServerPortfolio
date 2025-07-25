#include "pch.h"
#include "CPlayer.h"
#include "CEchoContents.h"
#include "EchoProtocol.h"
#include "CEchoServer.h"
#include "CGenPacket.h"

namespace EchoServer::Contents
{
    void CEchoContents::OnEnter(const UINT64 sessionID, void *pObject) noexcept
    {
        m_umapSessions.insert(std::make_pair(sessionID, pObject));
    }

    void CEchoContents::OnLeave(const UINT64 sessionID) noexcept
    {
        auto it = m_umapSessions.find(sessionID);
        if (it == m_umapSessions.end())
        {
            __debugbreak();
            return;
        }

        EchoServer::Objects::CPlayer *pPlayer 
            = reinterpret_cast<EchoServer::Objects::CPlayer *>(it->second);
        EchoServer::Objects::CPlayer::Free(pPlayer);
        m_umapSessions.erase(sessionID);
    }

    NetworkLib::Contents::RECV_RET CEchoContents::OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept
    {
        WORD wordType;
        *message >> wordType;

        EchoServer::Protocol::PACKET_TYPE type = static_cast<EchoServer::Protocol::PACKET_TYPE>(wordType);
        switch (type)
        {
        case EchoServer::Protocol::PACKET_TYPE::CS_GAME_RES_ECHO:
        {
            auto it = m_umapSessions.find(sessionID);
            if (it == m_umapSessions.end())
            {
                // Player 찾기 실패
                return NetworkLib::Contents::RECV_RET::RECV_FALSE;
            }

            EchoServer::Objects::CPlayer *pPlayer 
                = reinterpret_cast<EchoServer::Objects::CPlayer *>(it->second);

            INT64 accountNo;
            LONGLONG sendTick;

            if (message->GetDataSize() != sizeof(INT64) + sizeof(LONGLONG))
                return NetworkLib::Contents::RECV_RET::RECV_FALSE;

            *message >> accountNo >> sendTick;

            if (accountNo != pPlayer->m_iAccountNo)
                return NetworkLib::Contents::RECV_RET::RECV_FALSE;

            if (message->GetDataSize() != 0)
                return NetworkLib::Contents::RECV_RET::RECV_FALSE;

            pPlayer->m_dwPrevRecvTime = timeGetTime();

            NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pEchoRes
                = EchoServer::Protocol::CGenPacket::makePacketResEcho(accountNo, sendTick);
            NetworkLib::Core::Net::Server::g_NetServer->SendPacket(sessionID, pEchoRes);

            return NetworkLib::Contents::RECV_RET::RECV_TRUE;
        }
            break;

        case EchoServer::Protocol::PACKET_TYPE::CS_GAME_REQ_HEARTBEAT:
        {
            if (message->GetDataSize() != 0)
                return NetworkLib::Contents::RECV_RET::RECV_FALSE;

            auto it = m_umapSessions.find(sessionID);
            if (it == m_umapSessions.end())
            {
                // Player 찾기 실패
                return NetworkLib::Contents::RECV_RET::RECV_FALSE;
            }

            EchoServer::Objects::CPlayer *pPlayer
                = reinterpret_cast<EchoServer::Objects::CPlayer *>(it->second);
            pPlayer->m_dwPrevRecvTime = timeGetTime();

            return NetworkLib::Contents::RECV_RET::RECV_TRUE;
        }
            break;
        default:
            break;
        }


        return NetworkLib::Contents::RECV_RET::RECV_FALSE;
    }

    void CEchoContents::OnLoopEnd() noexcept
    {
    }
}