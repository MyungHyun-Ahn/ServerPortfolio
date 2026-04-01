#include "Servers/FServerFactory.h"

#include "Servers/FIocpServer.h"
#include "Servers/FStubServer.h"

namespace GameServer::NetworkLib
{
	std::unique_ptr<IServer> FServerFactory::Create(EBackendKind backendKind)
	{
		switch (backendKind)
		{
		case EBackendKind::Iocp:
			return std::make_unique<FIocpServer>();
		case EBackendKind::Rio:
		case EBackendKind::BoostAsio:
			return std::make_unique<FStubServer>(backendKind);
		default:
			return nullptr;
		}
	}
}
