#pragma once

namespace MonitoringClientLib
{
	namespace Config
	{
		extern INT SERVER_NO;
		extern std::string IP;
		extern USHORT PORT;

		void Load(MHLib::utils::CFileLoader &loader);
	}
}

