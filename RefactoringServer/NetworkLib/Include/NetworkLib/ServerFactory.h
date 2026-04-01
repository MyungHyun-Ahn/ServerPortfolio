#pragma once

#include "IServer.h"

#include <memory>

namespace GameServer::NetworkLib
{
	class FServerFactory
	{
	public:
		static std::unique_ptr<IServer> Create(EBackendKind backendKind);
	};
}
