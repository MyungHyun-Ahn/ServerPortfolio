#include "Servers/FStubServer.h"

#include <iostream>

namespace GameServer::NetworkLib
{
	FStubServer::FStubServer(EBackendKind backendKind)
		: m_backendKind(backendKind)
	{
	}

	bool FStubServer::Start(const SServerConfig&, IApplicationHandler&)
	{
		std::cerr << "선택한 백엔드는 아직 구현하지 않았습니다.\n";
		return false;
	}

	void FStubServer::Stop()
	{
	}

	bool FStubServer::Send(std::uint64_t, const char*, std::int32_t)
	{
		return false;
	}

	EBackendKind FStubServer::GetBackendKind() const
	{
		return m_backendKind;
	}
}
