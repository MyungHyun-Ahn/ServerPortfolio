#pragma once

// 사용자 입장에서 해당 헤더만 include 하면 사용할 수 있게 만들고 싶기 때문
#include "NetLibDefine.h"
#include "LanSetting.h"
#include "CoreUtils.h"

namespace NetworkLib::Core::Lan::Client
{
#pragma warning(disable : 6387) // ConnectEx Param 경고

		class CLanClientManager;

		class CLanSession
		{
		public:
			friend class CLanClient;
			friend class CLanClientManager;

			CLanSession() noexcept
				: m_sSessionSocket(INVALID_SOCKET)
				, m_uiSessionID(0)
			{
				m_pMyOverlappedStartAddr = s_OverlappedPool.Alloc();
			}

			~CLanSession() noexcept
			{
			}

			void Init(UINT64 sessionID, INT clientMgrIndex) noexcept
			{
				m_uiSessionID = sessionID;
				m_ClientMgrIndex = clientMgrIndex;
				InterlockedExchange(&m_iSendFlag, FALSE);
				InterlockedExchange(&m_iCacelIoCalled, FALSE);
				InterlockedAnd(&m_iIOCountAndRelease, ~RELEASE_FLAG);
			}

			void Init(SOCKET socket, UINT64 sessionID, INT clientMgrIndex) noexcept
			{
				m_sSessionSocket = socket;
				m_uiSessionID = sessionID;
				m_ClientMgrIndex = clientMgrIndex;
				InterlockedExchange(&m_iSendFlag, FALSE);
				InterlockedExchange(&m_iCacelIoCalled, FALSE);
				InterlockedAnd(&m_iIOCountAndRelease, ~RELEASE_FLAG);
			}

			void Clear() noexcept;

			void RecvCompleted(int size) noexcept;

			bool SendPacket(NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *message) noexcept;
			void SendCompleted(int size) noexcept;

			bool PostRecv() noexcept;
			bool PostSend(bool isPQCS = FALSE) noexcept;

		private:
			// 패딩 계산해서 세션 크기 최적화
		// + Interlock 사용하는 변수들은 캐시라인 띄워놓기
		// Release + IoCount
			LONG m_iIOCountAndRelease = RELEASE_FLAG;
			LONG m_iSendCount = 0;
			
			NetworkLib::DataStructures::CRingBuffer<4096> m_RecvBuffer;

			// CRecvBuffer *m_pRecvBuffer = nullptr;
			SOCKET m_sSessionSocket;
			UINT64 m_uiSessionID;
			WCHAR		m_ClientAddrBuffer[16];
			// --- 64

			char	    m_AcceptBuffer[64];
			// --- 128

			LONG		m_iSendFlag = FALSE;
			USHORT		m_ClientPort;
			char		m_dummy01[2]; // 패딩 계산용
			// 최대 무조건 1개 -> 있거나 없거나
			// CSerializableBufferView<SERVER_TYPE::LAN> *m_pDelayedBuffer = nullptr;
			MHLib::containers::CLFQueue<NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *> m_lfSendBufferQueue;
			// m_pMyOverlappedStartAddr
			//  + 0 : ACCEPTEX
			//  + 1 : RECV
			//  + 2 : SEND
			OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
			NetworkLib::DataStructures::CSerializableBuffer<SERVER_TYPE::LAN> *m_arrPSendBufs[NetworkLib::Core::Utils::WSASEND_MAX_BUFFER_COUNT]; // 8 * 32 = 256

			LONG m_iCacelIoCalled = FALSE;

			INT m_ClientMgrIndex;


			USE_TLS_POOL_WITH_INIT(CLanSession, s_sSessionPool, Clear)

			inline static constexpr LONG RELEASE_FLAG = 0x80000000;
			inline static constexpr LONG ENQUEUE_FLAG = 0x80000000;

			inline static NetworkLib::DataStructures::COverlappedAllocator<> s_OverlappedPool;
		};

		class CLanClient
		{
		public:
			friend class CLanClientManager;

			BOOL Init(INT maxSessionCount) noexcept;

			inline LONG GetSessionCount() const noexcept { return m_iSessionCount; }

			void SendPacket(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *sBuffer) noexcept;
			void EnqueuePacket(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *sBuffer) noexcept;

			BOOL Disconnect(const UINT64 sessionID, BOOL isPQCS = FALSE) noexcept;
			BOOL ReleaseSession(CLanSession *pSession, BOOL isPQCS = FALSE) noexcept;
			BOOL ReleaseSessionPQCS(CLanSession *pSession) noexcept;

			virtual void OnConnect(const UINT64 sessionID) noexcept = 0;
			virtual void OnDisconnect(const UINT64 sessionID) noexcept = 0;
			virtual void OnRecv(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *message) noexcept = 0;

			BOOL PostConnectEx(const CHAR *ip, const USHORT port) noexcept;
			BOOL ConnectExCompleted(CLanSession *pSession) noexcept;

		private:
			INT m_ClientMgrIndex;

			// Session
			LONG					m_iSessionCount = 0;
			LONG64					m_iCurrentID = 0;
			MHLib::containers::CLFStack<USHORT>		m_stackDisconnectIndex;

			INT			m_iMaxSessionCount;
			CLanSession **m_arrPSessions;
		};

		class CLanClientManager
		{
		public:
			friend class CLanSession;
			friend class CLanClient;

			// IOCP 핸들 생성 등
			BOOL Init() noexcept;

			BOOL Start() noexcept;
			BOOL Wait() noexcept;

			// 서버 처음 실행시에만 등록 돌고있을 때 추가는 없음
			inline void RegisterClient(CLanClient *client) noexcept
			{
				client->m_ClientMgrIndex = m_ClientSize++;
				m_arrNetClients.push_back(client);
			}

			inline void RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionID);

			inline BOOL ConnectEx(SOCKET sock, sockaddr *name, int nameLen, LPOVERLAPPED lpOverlapped)
			{
				return m_lpfnConnectEx(sock, name, nameLen, nullptr, 0, nullptr, lpOverlapped);
			}

		public:
			int WorkerThread() noexcept;

		private:
			// Connector
			LPFN_CONNECTEX m_lpfnConnectEx = NULL;
			GUID m_guidConnectEx = WSAID_CONNECTEX;

			// Worker
			UINT32					m_uiMaxWorkerThreadCount;
			BOOL					m_bIsWorkerRun = TRUE;
			std::vector<HANDLE>		m_arrWorkerThreads;

			INT m_ClientSize = 0;
			std::vector<CLanClient *>		m_arrNetClients;
			HANDLE							m_hIOCPHandle = INVALID_HANDLE_VALUE;
		};

		extern CLanClientManager *g_netClientMgr;
}