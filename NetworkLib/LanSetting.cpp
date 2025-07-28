#include "pch.h"
#include "LanSetting.h"


namespace NetworkLib::Core::Lan
{
	namespace Client::Config
	{
		INT IOCP_WORKER_THREAD = 1;
		INT IOCP_ACTIVE_THREAD = 1;
		INT USE_ZERO_COPY = 1;

		void Load(MHLib::utils::CFileLoader &loader)
		{
			loader.Load(L"Client", L"USE_ZERO_COPY", &USE_ZERO_COPY);
			loader.Load(L"Client", L"IOCP_WORKER_THREAD", &IOCP_WORKER_THREAD);
			loader.Load(L"Client", L"IOCP_ACTIVE_THREAD", &IOCP_ACTIVE_THREAD);
		}
	}
}
