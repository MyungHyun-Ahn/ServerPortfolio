#pragma once

#include "NetworkLib/CBaseContents.h"

namespace WorldServer::Contents
{
	class CEntranceContents : public NetworkLib::Contents::CBaseContents
	{
	public:
		void OnEnter(const UINT64 sessionID, void *pObject) noexcept override;
		void OnLeave(const UINT64 sessionID) noexcept override;
		NetworkLib::Contents::RECV_RET OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *message, int delayFrame) noexcept override;
		void OnLoopEnd() noexcept override;
	};
}
