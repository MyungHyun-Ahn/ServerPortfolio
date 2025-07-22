#pragma once


namespace NetworkLib::Core::Net
{
	// Lib ��ũ�� CPP ������Ʈ���� ���� ���� �о ���� �ʿ�
	namespace Server
	{
		extern BYTE PACKET_KEY;
		extern BYTE PACKET_CODE;

		extern std::string openIP;
		extern USHORT openPort;

		extern INT IOCP_WORKER_THREAD;
		extern INT IOCP_ACTIVE_THREAD;
		extern INT USE_ZERO_COPY;
		extern INT MAX_SESSION_COUNT;
		extern INT ACCEPTEX_COUNT;

		extern INT CONTENTS_THREAD_COUNT;
		extern INT MAX_CONTENTS_FPS;
		extern INT DELAY_FRAME;
	}
}