#pragma once

namespace MonitoringClientLib::Protocol
{
	enum class PACKET_TYPE
	{
		SS_MONITOR = 20000,
		SS_MONITOR_LOGIN,
		SS_MONITOR_DATA_UPDATE,
	};

	enum class MONITOR_DATA_UPDATE
	{
		LOGIN_SERVER_RUN = 1,		// �α��μ��� ���࿩�� ON / OFF
		LOGIN_SERVER_CPU = 2,		// �α��μ��� CPU ����
		LOGIN_SERVER_MEM = 3,		// �α��μ��� �޸� ��� MByte
		LOGIN_SESSION = 4,		// �α��μ��� ���� �� (���ؼ� ��)
		LOGIN_AUTH_TPS = 5,		// �α��μ��� ���� ó�� �ʴ� Ƚ��
		LOGIN_PACKET_POOL = 6,		// �α��μ��� ��ŶǮ ��뷮


		GAME_SERVER_RUN = 10,		// GameServer ���� ���� ON / OFF
		GAME_SERVER_CPU = 11,		// GameServer CPU ����
		GAME_SERVER_MEM = 12,		// GameServer �޸� ��� MByte
		GAME_SESSION = 13,		// ���Ӽ��� ���� �� (���ؼ� ��)
		GAME_AUTH_PLAYER = 14,		// ���Ӽ��� AUTH MODE �÷��̾� ��
		GAME_GAME_PLAYER = 15,		// ���Ӽ��� GAME MODE �÷��̾� ��
		GAME_ACCEPT_TPS = 16,		// ���Ӽ��� Accept ó�� �ʴ� Ƚ��
		GAME_PACKET_RECV_TPS = 17,		// ���Ӽ��� ��Ŷó�� �ʴ� Ƚ��
		GAME_PACKET_SEND_TPS = 18,		// ���Ӽ��� ��Ŷ ������ �ʴ� �Ϸ� Ƚ��
		GAME_DB_WRITE_TPS = 19,		// ���Ӽ��� DB ���� �޽��� �ʴ� ó�� Ƚ��
		GAME_DB_WRITE_MSG = 20,		// ���Ӽ��� DB ���� �޽��� ť ���� (���� ��)
		GAME_AUTH_THREAD_FPS = 21,		// ���Ӽ��� AUTH ������ �ʴ� ������ �� (���� ��)
		GAME_GAME_THREAD_FPS = 22,		// ���Ӽ��� GAME ������ �ʴ� ������ �� (���� ��)
		GAME_PACKET_POOL = 23,		// ���Ӽ��� ��ŶǮ ��뷮

		CHAT_SERVER_RUN = 30,		// ä�ü��� ChatServer ���� ���� ON / OFF
		CHAT_SERVER_CPU = 31,		// ä�ü��� ChatServer CPU ����
		CHAT_SERVER_MEM = 32,		// ä�ü��� ChatServer �޸� ��� MByte
		CHAT_SESSION = 33,		// ä�ü��� ���� �� (���ؼ� ��)
		CHAT_PLAYER = 34,		// ä�ü��� �������� ����� �� (���� ������)
		CHAT_UPDATE_TPS = 35,		// ä�ü��� UPDATE ������ �ʴ� �ʸ� Ƚ��
		CHAT_PACKET_POOL = 36,		// ä�ü��� ��ŶǮ ��뷮
		CHAT_UPDATEMSG_POOL = 37,		// ä�ü��� UPDATE MSG Ǯ ��뷮


		MONITOR_CPU_TOTAL = 40,				// ������ǻ�� CPU ��ü ����
		MONITOR_NONPAGED_MEMORY = 41,		// ������ǻ�� �������� �޸� MByte
		MONITOR_NETWORK_RECV = 42,			// ������ǻ�� ��Ʈ��ũ ���ŷ� KByte
		MONITOR_NETWORK_SEND = 43,			// ������ǻ�� ��Ʈ��ũ �۽ŷ� KByte
		MONITOR_AVAILABLE_MEMORY = 44,		// ������ǻ�� ��밡�� �޸�
	};

	enum class MONITOR_TOOL_RES_LOGIN
	{
		OK = 1,		// �α��� ����
		ERR_NOSERVER = 2,		// �����̸� ���� (��Ī�̽�)
		ERR_SESSIONKEY = 3,		// �α��� ����Ű ����
	};
}