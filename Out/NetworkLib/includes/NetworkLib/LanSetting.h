#pragma once

namespace NetworkLib::Core::Lan
{
	// Lib 링크한 CPP 프로젝트에서 설정 파일 읽어서 세팅 필요
	namespace Client
	{
		extern INT USE_ZERO_COPY;
		extern INT IOCP_WORKER_THREAD;
		extern INT IOCP_ACTIVE_THREAD;
	}
}
