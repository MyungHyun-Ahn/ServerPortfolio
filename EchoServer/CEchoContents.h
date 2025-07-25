#pragma once

namespace EchoServer::Contents
{
	class CEchoContents : public NetworkLib::Contents::CBaseContents
	{
		// CBaseContents을(를) 통해 상속됨
		void OnEnter(const UINT64 sessionID, void *pObject) noexcept override;
		void OnLeave(const UINT64 sessionID) noexcept override;
		NetworkLib::Contents::RECV_RET OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept override;
		void OnLoopEnd() noexcept override;
	};
}