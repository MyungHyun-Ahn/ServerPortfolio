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
		LOGIN_SERVER_RUN = 1,		// 로그인서버 실행여부 ON / OFF
		LOGIN_SERVER_CPU = 2,		// 로그인서버 CPU 사용률
		LOGIN_SERVER_MEM = 3,		// 로그인서버 메모리 사용 MByte
		LOGIN_SESSION = 4,		// 로그인서버 세션 수 (컨넥션 수)
		LOGIN_AUTH_TPS = 5,		// 로그인서버 인증 처리 초당 횟수
		LOGIN_PACKET_POOL = 6,		// 로그인서버 패킷풀 사용량


		GAME_SERVER_RUN = 10,		// GameServer 실행 여부 ON / OFF
		GAME_SERVER_CPU = 11,		// GameServer CPU 사용률
		GAME_SERVER_MEM = 12,		// GameServer 메모리 사용 MByte
		GAME_SESSION = 13,		// 게임서버 세션 수 (컨넥션 수)
		GAME_AUTH_PLAYER = 14,		// 게임서버 AUTH MODE 플레이어 수
		GAME_GAME_PLAYER = 15,		// 게임서버 GAME MODE 플레이어 수
		GAME_ACCEPT_TPS = 16,		// 게임서버 Accept 처리 초당 횟수
		GAME_PACKET_RECV_TPS = 17,		// 게임서버 패킷처리 초당 횟수
		GAME_PACKET_SEND_TPS = 18,		// 게임서버 패킷 보내기 초당 완료 횟수
		GAME_DB_WRITE_TPS = 19,		// 게임서버 DB 저장 메시지 초당 처리 횟수
		GAME_DB_WRITE_MSG = 20,		// 게임서버 DB 저장 메시지 큐 개수 (남은 수)
		GAME_AUTH_THREAD_FPS = 21,		// 게임서버 AUTH 스레드 초당 프레임 수 (루프 수)
		GAME_GAME_THREAD_FPS = 22,		// 게임서버 GAME 스레드 초당 프레임 수 (루프 수)
		GAME_PACKET_POOL = 23,		// 게임서버 패킷풀 사용량

		CHAT_SERVER_RUN = 30,		// 채팅서버 ChatServer 실행 여부 ON / OFF
		CHAT_SERVER_CPU = 31,		// 채팅서버 ChatServer CPU 사용률
		CHAT_SERVER_MEM = 32,		// 채팅서버 ChatServer 메모리 사용 MByte
		CHAT_SESSION = 33,		// 채팅서버 세션 수 (컨넥션 수)
		CHAT_PLAYER = 34,		// 채팅서버 인증성공 사용자 수 (실제 접속자)
		CHAT_UPDATE_TPS = 35,		// 채팅서버 UPDATE 스레드 초당 초리 횟수
		CHAT_PACKET_POOL = 36,		// 채팅서버 패킷풀 사용량
		CHAT_UPDATEMSG_POOL = 37,		// 채팅서버 UPDATE MSG 풀 사용량


		MONITOR_CPU_TOTAL = 40,				// 서버컴퓨터 CPU 전체 사용률
		MONITOR_NONPAGED_MEMORY = 41,		// 서버컴퓨터 논페이지 메모리 MByte
		MONITOR_NETWORK_RECV = 42,			// 서버컴퓨터 네트워크 수신량 KByte
		MONITOR_NETWORK_SEND = 43,			// 서버컴퓨터 네트워크 송신량 KByte
		MONITOR_AVAILABLE_MEMORY = 44,		// 서버컴퓨터 사용가능 메모리
	};

	enum class MONITOR_TOOL_RES_LOGIN
	{
		OK = 1,		// 로그인 성공
		ERR_NOSERVER = 2,		// 서버이름 오류 (매칭미스)
		ERR_SESSIONKEY = 3,		// 로그인 세션키 오류
	};
}