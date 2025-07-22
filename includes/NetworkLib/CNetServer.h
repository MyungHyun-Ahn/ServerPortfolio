#pragma once

namespace NetworkLib::Core::Utils
{
	class CAccessor;
}


namespace NetworkLib::Contents
{
	class CBaseContents;
}

namespace NetworkLib::Core::Net::Server
{
	class CNetSession
	{
	public:
		friend class CNetServer;
		friend class NetworkLib::Contents::CBaseContents;
		friend struct ContentsFrameTask;

		CNetSession() noexcept
			: m_sSessionSocket(INVALID_SOCKET)
			, m_uiSessionID(0)
		{
			// + 0 : AcceptEx
			// + 1 : Recv
			// + 2 : Send
			m_pMyOverlappedStartAddr = s_OverlappedPool.Alloc();
		}

		~CNetSession() noexcept
		{
		}

		void Init(UINT64 sessionID) noexcept
		{
			m_uiSessionID = sessionID;
			InterlockedExchange(&m_iSendFlag, FALSE);
			InterlockedExchange(&m_iCacelIoCalled, FALSE);
			InterlockedAnd(&m_iIOCountAndRelease, ~RELEASE_FLAG);
		}

		void Init(SOCKET socket, UINT64 sessionID) noexcept
		{
			m_sSessionSocket = socket;
			m_uiSessionID = sessionID;
			InterlockedExchange(&m_iSendFlag, FALSE);
			InterlockedExchange(&m_iCacelIoCalled, FALSE);
			InterlockedAnd(&m_iIOCountAndRelease, ~RELEASE_FLAG);
		}

		void Clear() noexcept;

		void RecvCompleted(int size) noexcept;

		// 인큐할 때 직렬화 버퍼의 포인터를 인큐


		inline bool SendPacket(DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *message) noexcept
		{
			message->IncreaseRef();
			{
				PROFILE_BEGIN(0, "SEND_MSQ ENQUEUE");
				m_lfSendBufferQueue.Enqueue(message);
			}
			if (m_lfSendBufferQueue.GetUseSize() > Utils::WSASEND_MAX_BUFFER_COUNT)
				return FALSE;

			InterlockedOr(&m_iSendFlag, ENQUEUE_FLAG);
			return TRUE;
		}

		void SendCompleted(int size) noexcept;

		bool PostRecv() noexcept;
		bool PostSend(bool isSendFlag = FALSE) noexcept;

	public:
		inline static CNetSession *Alloc() noexcept
		{
			CNetSession *pSession = s_sSessionPool.Alloc();
			return pSession;
		}

		inline static void Free(CNetSession *delSession) noexcept
		{
			delSession->Clear();
			s_sSessionPool.Free(delSession);
		}

		inline static LONG GetPoolCapacity() noexcept { return s_sSessionPool.GetCapacity(); }
		inline static LONG GetPoolUsage() noexcept { return s_sSessionPool.GetUseCount(); }

	private:
		// 패딩 계산해서 세션 크기 최적화
		// + Interlock 사용하는 변수들은 캐시라인 띄워놓기
		// Release + IoCount
		LONG m_iIOCountAndRelease = RELEASE_FLAG;
		LONG m_iSendCount = 0;
		DataStructures::CRingBuffer<4096> m_RecvBuffer;
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
		// CSerializableBufferView<FALSE> *m_pDelayedBuffer = nullptr;
		MHLib::containers::CLFQueue<DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *> m_lfSendBufferQueue;
		// m_pMyOverlappedStartAddr
		//  + 0 : ACCEPTEX
		//  + 1 : RECV
		//  + 2 : SEND
		OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
		DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *m_arrPSendBufs[Utils::WSASEND_MAX_BUFFER_COUNT]; // 8 * 32 = 256

		LONG m_iCacelIoCalled = FALSE;

		// Recv 큐
		MHLib::containers::CLFQueue<DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *> m_RecvMsgQueue;
		// LONG m_ContentStatus = FALSE;

		// 총 460바이트

		// ContentPtr
		NetworkLib::Contents::CBaseContents * m_pCurrentContent = nullptr;

		inline static MHLib::memory::CTLSMemoryPoolManager<CNetSession, 16, 4> s_sSessionPool = MHLib::memory::CTLSMemoryPoolManager<CNetSession, 16, 4>();
		inline static constexpr LONG RELEASE_FLAG = 0x80000000;
		inline static constexpr LONG ENQUEUE_FLAG = 0x80000000;

		inline static DataStructures::COverlappedAllocator<> s_OverlappedPool;
	};


	class CNetServer
	{
	public:
		friend class CNetSession;
		friend class NetworkLib::Contents::CBaseContents;
		friend class Utils::CAccessor;
		// friend struct ContentsFrameEvent;

		BOOL Start(const CHAR *openIP, const USHORT port) noexcept;
		void Stop();

		inline LONG GetSessionCount() const noexcept { return m_monitoringTargets.sessionCount; }

		void SendPacket(const UINT64 sessionID, DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *sBuffer) noexcept;
		// Send 시도는 하지 않음
		void EnqueuePacket(const UINT64 sessionID, DataStructures::CSerializableBuffer<SERVER_TYPE::NET> *sBuffer) noexcept;
		void SendPQCS(const CNetSession *pSession);

		BOOL Disconnect(const UINT64 sessionID, BOOL isPQCS = FALSE) noexcept;
		BOOL ReleaseSession(CNetSession *pSession, BOOL isPQCS = FALSE) noexcept;
		BOOL ReleaseSessionPQCS(CNetSession *pSession) noexcept;

		virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept = 0;

		virtual void OnAccept(const UINT64 sessionID) noexcept = 0;
		virtual void OnClientLeave(const UINT64 sessionID) noexcept = 0;


		virtual void OnError(int errorcode, WCHAR *errMsg) noexcept = 0;

		// AcceptEx
		void FristPostAcceptEx() noexcept;
		BOOL PostAcceptEx(INT index) noexcept;
		BOOL AcceptExCompleted(CNetSession *pSession) noexcept;

	public:
		int WorkerThread() noexcept;

		void RegisterSystemTimerEvent();
		virtual void RegisterContentTimerEvent() noexcept = 0;

	private:
		// Session
		LONG64					m_iCurrentID = 0;

		USHORT					m_usMaxSessionCount;
		CNetSession **m_arrPSessions;
		MHLib::containers::CLFStack<USHORT>		m_stackDisconnectIndex;

		// Worker
		SOCKET					m_sListenSocket = INVALID_SOCKET;
		UINT32					m_uiMaxWorkerThreadCount;
		BOOL					m_bIsWorkerRun = TRUE;
		std::vector<HANDLE>		m_arrWorkerThreads;

		// Accepter
		LPFN_ACCEPTEX					m_lpfnAcceptEx = NULL;
		GUID							m_guidAcceptEx = WSAID_ACCEPTEX;
		CNetSession **m_arrAcceptExSessions;

		LPFN_GETACCEPTEXSOCKADDRS		m_lpfnGetAcceptExSockaddrs = NULL;
		GUID							m_guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		// IOCP 핸들
		HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;
		LONG m_isStop = FALSE;

		// 모니터링 항목
		NetworkLib::Core::Monitoring::ServerMonitoringTargets m_monitoringTargets;
	};

	extern CNetServer *g_NetServer;
}