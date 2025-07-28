#include "pch.h"
#include "MonitoringSetting.h"

namespace MonitoringClientLib
{
	namespace Config
	{
		INT SERVER_NO;
		std::string IP;
		USHORT PORT;

		void Load(MHLib::utils::CFileLoader &loader)
		{
			loader.Load(L"MonitoringClient", L"SERVER_NO", &SERVER_NO);
			loader.Load(L"MonitoringClient", L"IP", &IP);
			loader.Load(L"MonitoringClient", L"PORT", &PORT);
		}
	}
}