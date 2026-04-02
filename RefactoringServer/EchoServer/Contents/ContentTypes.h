#pragma once

#include "ContentsRuntime/Core/ContentTypes.h"

namespace EchoServer::Contents
{
	inline constexpr ContentsRuntime::Core::FContentId kAuthContentId = 1;
	inline constexpr ContentsRuntime::Core::FContentId kEchoContentId = 2;

	struct SRuntimeOptions
	{
		int sendThreadCount = 1;
		int responsesPerThread = 1;
		bool logPackets = false;
		bool bootstrapTrace = false;
		bool enablePagePool = true;
		std::uint32_t pageSize = 4096;
	};
}
