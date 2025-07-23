#include "pch.h"
#include "CNetServer.h"
#include "CBaseContents.h"
#include "PrivateHeader/MonitoringTarget.h"
#include "PrivateHeader/CAccessor.h"
#include "PrivateHeader/CCoreLibInit.h"

namespace NetworkLib::Core::Net::Server
{
	CNetServer *g_NetServer = nullptr;

	void CNetSession::RecvCompleted(int size) noexcept
	{
		m_RecvBuffer.MoveRear(size);
		int currentUseSize = m_RecvBuffer.GetUseSize();
		while (currentUseSize > 0)
		{
			if (currentUseSize < sizeof(NetworkLib::Core::Utils::Net::Header))
				break;

			NetworkLib::Core::Utils::Net::Header packetHeader;
			m_RecvBuffer.Peek((char *)&packetHeader, sizeof(NetworkLib::Core::Utils::Net::Header));

			if (packetHeader.code != PACKET_CODE)
			{
				g_NetServer->Disconnect(m_uiSessionID);
				return;
			}
			if (packetHeader.len > (int)NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::DEFINE::PACKET_MAX_SIZE - sizeof(NetworkLib::Core::Utils::Net::Header) || packetHeader.len >= 65535 || packetHeader.len >= m_RecvBuffer.GetCapacity())
			{
				g_NetServer->Disconnect(m_uiSessionID);
				return;
			}
			if (m_RecvBuffer.GetUseSize() < sizeof(NetworkLib::Core::Utils::Net::Header) + packetHeader.len)
				break;

			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *view = NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Alloc();
			m_RecvBuffer.MoveFront(sizeof(NetworkLib::Core::Utils::Net::Header));
			view->EnqueueHeader((char *)&packetHeader, sizeof(NetworkLib::Core::Utils::Net::Header));

			m_RecvBuffer.Dequeue(view->GetContentBufferPtr(), packetHeader.len);
			view->MoveWritePos(packetHeader.len);

			MHLib::scurity::CEncryption::Decoding(view->GetBufferPtr() + 4, view->GetFullSize() - 4, packetHeader.randKey);
			BYTE dataCheckSum = MHLib::scurity::CEncryption::CalCheckSum(view->GetContentBufferPtr(), view->GetDataSize());
			BYTE headerCheckSum = *(view->GetBufferPtr() + offsetof(NetworkLib::Core::Utils::Net::Header, checkSum));
			if (dataCheckSum != headerCheckSum)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Free(view);
				g_NetServer->Disconnect(m_uiSessionID);
				return;
			}

			view->IncreaseRef();
			InterlockedIncrement(&g_NetServer->m_monitoringTargets->recvTPS);
			{
				// PROFILE_BEGIN(0, "RECV_MSG ENQUEUE");
				m_RecvMsgQueue.Enqueue(view);
			}
			currentUseSize = m_RecvBuffer.GetUseSize();
		}
	}

	// m_iSendCount�� �ϰ� �Ҵ� ������ ����
	// * ����ŷ I/O�� ���� Send�� ��û�� �����ͺ��� �� ������ ��Ȳ�� �߻� ����
	// * �񵿱� I/O�� ������ ���� ������ �Ϸ� ������ ������

	void CNetSession::SendCompleted(int size) noexcept
	{
		// InterlockedAdd(&g_monitor.m_lSendTPS, m_iSendCount);

		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		InterlockedExchange(&m_iSendFlag, FALSE);

		PostSend();
	}

	bool CNetSession::PostRecv() noexcept
	{
		int errVal;
		int retVal;

		// ���⼭�� ���� Ref ������ �ʿ�
		// * WSARecv���� Ref ������ �Ұ���

		InterlockedIncrement(&m_iIOCountAndRelease);
		if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			InterlockedDecrement(&m_iIOCountAndRelease);
			return FALSE;
		}

		WSABUF wsaBuf[2];
		wsaBuf[0].buf = m_RecvBuffer.GetRearPtr();
		wsaBuf[0].len = m_RecvBuffer.DirectEnqueueSize();
		wsaBuf[1].buf = m_RecvBuffer.GetPQueuePtr();
		wsaBuf[1].len = m_RecvBuffer.GetFreeSize() - wsaBuf[0].len;

		ZeroMemory((m_pMyOverlappedStartAddr + 1), sizeof(OVERLAPPED));

		DWORD flag = 0;
		{
			// PROFILE_BEGIN(0, "WSARecv");
			retVal = WSARecv(m_sSessionSocket, wsaBuf, 2, nullptr, &flag, (LPWSAOVERLAPPED)(m_pMyOverlappedStartAddr + 1), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSARecv() Error : %d", errVal);

				// ��� ���⼱ 0�� �� ���� ����
				// ��ȯ���� �����ص� ��
				if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				if (m_iCacelIoCalled)
				{
					CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	bool CNetSession::PostSend(bool isSendFlag) noexcept
	{
		int errVal;
		int retVal;
		int sendUseSize;

		if (!isSendFlag)
		{
			sendUseSize = m_lfSendBufferQueue.GetUseSize();
			if (sendUseSize <= 0)
			{
				return FALSE;
			}

			if ((InterlockedExchange(&m_iSendFlag, TRUE) & ~(ENQUEUE_FLAG)) == TRUE)
			{
				return FALSE;
			}
		}

		sendUseSize = m_lfSendBufferQueue.GetUseSize();
		if (sendUseSize <= 0)
		{
			LONG beforeSendFlag = InterlockedCompareExchange(&m_iSendFlag, FALSE, TRUE);
			if ((beforeSendFlag & ENQUEUE_FLAG) == ENQUEUE_FLAG) // �ֻ��� ��Ʈ ��������
			{
				PostSend(TRUE);
				return FALSE;
			}
			return FALSE;
		}

		InterlockedIncrement(&m_iIOCountAndRelease);
		if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			InterlockedDecrement(&m_iIOCountAndRelease);
			InterlockedExchange(&m_iSendFlag, FALSE);
			return FALSE;
		}

		WSABUF wsaBuf[Utils::WSASEND_MAX_BUFFER_COUNT];

		m_iSendCount = min(sendUseSize, Utils::WSASEND_MAX_BUFFER_COUNT);
		// WSASEND_MAX_BUFFER_COUNT ��ŭ 1�ʿ� ��� �������� ī��Ʈ
		// �� ��ġ�� ���ٸ� �� �ø� ��
		if (m_iSendCount == Utils::WSASEND_MAX_BUFFER_COUNT)
			InterlockedIncrement(&g_NetServer->m_monitoringTargets->maxSendCount);

		int count;
		for (count = 0; count < m_iSendCount; count++)
		{
			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *sBuffer;
			// ������ ��
			{
				PROFILE_BEGIN(0, "SEND_MSQ DEQUEUE");
				if (!m_lfSendBufferQueue.Dequeue(&sBuffer))
				{
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
					MHLib::debug::CCrashDump::Crash();
				}
			}

			wsaBuf[count].buf = sBuffer->GetBufferPtr();
			wsaBuf[count].len = sBuffer->GetFullSize();

			m_arrPSendBufs[count] = sBuffer;
		}

		InterlockedAnd(&m_iSendFlag, TRUE);

		ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));
		InterlockedIncrement(&g_NetServer->m_monitoringTargets->sendCallTPS);
		{
			// PROFILE_BEGIN(0, "WSASend");
			retVal = WSASend(m_sSessionSocket, wsaBuf, m_iSendCount, nullptr, 0, (LPOVERLAPPED)(m_pMyOverlappedStartAddr + 2), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSASend() Error : %d", errVal);

				// ��� ���⼱ 0�� �� ���� ����
				// ��ȯ���� �����ص� ��
				if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				InterlockedIncrement(&g_NetServer->m_monitoringTargets->sendPendingCount);
				if (m_iCacelIoCalled)
				{
					CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	void CNetSession::Clear() noexcept
	{
		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pBuffer;
			// ������ ��
			{
				PROFILE_BEGIN(0, "SEND_MSQ DEQUEUE");
				if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
				{
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
					MHLib::debug::CCrashDump::Crash();
				}
			}

			// RefCount�� ���߰� 0�̶�� ���� �� ����
			if (pBuffer->DecreaseRef() == 0)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Free(pBuffer);
			}
		}

		useBufferSize = m_lfSendBufferQueue.GetUseSize();
		if (useBufferSize != 0)
		{
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"LFQueue is not empty Error");
		}

		useBufferSize = m_RecvMsgQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *pBuffer;
			{
				PROFILE_BEGIN(0, "RECV_MSQ DEQUEUE");
				if (!m_RecvMsgQueue.Dequeue(&pBuffer))
				{
					MHLib::debug::CCrashDump::Crash();
				}
			}

			if (pBuffer->DecreaseRef() == 0)
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET>::Free(pBuffer);
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
		// m_pRecvBuffer = nullptr;
		m_pCurrentContent = nullptr;
		m_RecvBuffer.Clear();
		InterlockedExchange(&m_iSendFlag, FALSE);
	}

	unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
	{
		return g_NetServer->WorkerThread();
	}

	BOOL CNetServer::Start(const CHAR *openIP, const USHORT port) noexcept
	{
		int retVal;
		int errVal;
		WSAData wsaData;

		m_monitoringTargets = new NetworkLib::Core::Monitoring::ServerMonitoringTargets;
		NetworkLib::Contents::CContentsThread::s_monitoringTargets = new NetworkLib::Core::Monitoring::ContentsThreadMonitoringTargets;

		m_usMaxSessionCount = MAX_SESSION_COUNT;

		m_arrPSessions = new CNetSession * [MAX_SESSION_COUNT];

		ZeroMemory((char *)m_arrPSessions, sizeof(CNetSession *) * MAX_SESSION_COUNT);

		m_arrAcceptExSessions = new CNetSession * [ACCEPTEX_COUNT];

		// ��Ŀ��Ʈ ���� ä���
		USHORT val = MAX_SESSION_COUNT - 1;
		for (int i = 0; i < MAX_SESSION_COUNT; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
			return FALSE;
		}

		m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_sListenSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSASocket() ���� : %d", errVal);
			return FALSE;
		}

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, openIP, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		retVal = bind(m_sListenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"bind() ���� : %d", errVal);
			return FALSE;
		}

		if (USE_ZERO_COPY)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
				return FALSE;
			}
		}

		// LINGER option ����
		LINGER ling{ 1, 0 };
		retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
			return FALSE;
		}

		// listen
		retVal = listen(m_sListenSocket, SOMAXCONN_HINT(65535));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"listen() ���� : %d", errVal);
			return FALSE;
		}

		// CP �ڵ� ����
		m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, IOCP_ACTIVE_THREAD);
		if (m_hIOCPHandle == NULL)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
			return FALSE;
		}

		CreateIoCompletionPort((HANDLE)m_sListenSocket, m_hIOCPHandle, (ULONG_PTR)0, 0);

		DWORD dwBytes;
		retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx), &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSAIoctl(lpfnAcceptEx) ���� : %d", errVal);
			return FALSE;
		}

		retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs, sizeof(m_guidGetAcceptExSockaddrs), &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSAIoctl(lpfnGetAcceptExSockaddrs) ���� : %d", errVal);
			return FALSE;
		}

		// AcceptEx ��û : TODO
		FristPostAcceptEx();

		for (int i = 0; i < CONTENTS_THREAD_COUNT; i++)
		{
			NetworkLib::Contents::CContentsThread * pContentsThread = new NetworkLib::Contents::CContentsThread;
			pContentsThread->Create();
		}

		NetworkLib::Contents::CContentsThread::RunAll();

		// CreateWorkerThread
		for (int i = 1; i <= IOCP_WORKER_THREAD; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, NetWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				errVal = GetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WorkerThread[%d] running fail.. : %d", i, errVal);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			MHLib::utils::g_Logger->WriteLogConsole(MHLib::utils::LOG_LEVEL::SYSTEM, L"[SYSTEM] WorkerThread[%d] running..", i);
		}

		RegisterSystemTimerEvent();
		RegisterContentTimerEvent();

		WaitForMultipleObjects((DWORD)m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);

		return TRUE;
	}

	void CNetServer::Stop()
	{
		// listen ���� �ݱ�
		InterlockedExchange(&m_isStop, TRUE);
		closesocket(m_sListenSocket);

		for (int i = 0; i < MAX_SESSION_COUNT; i++)
		{
			if (m_arrPSessions[i] != NULL)
			{
				Disconnect(m_arrPSessions[i]->m_uiSessionID);
			}
		}

		// while SessionCount 0�� ������
		while (true)
		{
			// ���� ���� ����
			if (m_monitoringTargets->sessionCount == 0)
			{
				m_bIsWorkerRun = FALSE;
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
	}

	void CNetServer::SendPacket(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *sBuffer) noexcept
	{
		CNetSession *pSession = m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (!sBuffer->GetIsEnqueueHeader())
		{
			sBuffer->m_isEnqueueHeader = true;
			NetworkLib::Core::Utils::Net::Header *header = (NetworkLib::Core::Utils::Net::Header *)sBuffer->GetBufferPtr();
			header->code = PACKET_CODE; // �ڵ�
			header->len = sBuffer->GetDataSize();
			header->randKey = 0;
			header->checkSum = MHLib::scurity::CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());

			// CheckSum ���� ��ȣȭ�ϱ� ����
			MHLib::scurity::CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetDataSize() + 1, header->randKey);
		}

		if (pSession->SendPacket(sBuffer))
			pSession->PostSend();
		else
			Disconnect(sessionID);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	// Sector ���� ���� ��쿡�� PQCS
	void CNetServer::EnqueuePacket(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::NET> *sBuffer) noexcept
	{
		CNetSession *pSession = m_arrPSessions[Utils::GetSessionIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		// ReleaseFlag�� �̹� ���� ��Ȳ
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (!sBuffer->GetIsEnqueueHeader())
		{
			sBuffer->m_isEnqueueHeader = true;
			NetworkLib::Core::Utils::Net::Header *header = (NetworkLib::Core::Utils::Net::Header *)sBuffer->GetBufferPtr();
			header->code = PACKET_CODE; // �ڵ�
			header->len = sBuffer->GetDataSize();
			header->randKey = 0;
			header->checkSum = MHLib::scurity::CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());

			// CheckSum ���� ��ȣȭ�ϱ� ����
			MHLib::scurity::CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetDataSize() + 1, header->randKey);
		}

		if (!pSession->SendPacket(sBuffer))
			Disconnect(sessionID);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession, TRUE);
		}
	}

	void CNetServer::SendPQCS(const CNetSession *pSession)
	{
		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)NetworkLib::DataStructures::IOOperation::SENDPOST);
	}

	BOOL CNetServer::Disconnect(const UINT64 sessionID, BOOL isPQCS) noexcept
	{
		CNetSession *pSession = m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(sessionID)];

		// ReleaseFlag�� �̹� ���� ��Ȳ
		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		if (InterlockedExchange(&pSession->m_iCacelIoCalled, TRUE) == TRUE)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		// Io ���� ����
		CancelIoEx((HANDLE)pSession->m_sSessionSocket, nullptr);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			// IOCount == 0 �̸� ���� �õ�
			ReleaseSession(pSession, isPQCS);
		}

		return TRUE;
	}

	BOOL CNetServer::ReleaseSession(CNetSession *pSession, BOOL isPQCS) noexcept
	{
		if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
		{
			return FALSE;
		}

		if (isPQCS)
		{
			PostQueuedCompletionStatus(m_hIOCPHandle, 0
				, (ULONG_PTR)pSession, (LPOVERLAPPED)NetworkLib::DataStructures::IOOperation::RELEASE_SESSION);
			return TRUE;
		}

		closesocket(pSession->m_sSessionSocket);
		USHORT index = NetworkLib::Core::Utils::GetSessionIndex(pSession->m_uiSessionID);
		UINT64 freeSessionId = pSession->m_uiSessionID;

		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);
		OnClientLeave(freeSessionId);

		CNetSession::Free(pSession);
		InterlockedDecrement(&m_monitoringTargets->sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetServer::ReleaseSessionPQCS(CNetSession *pSession) noexcept
	{
		USHORT index = NetworkLib::Core::Utils::GetSessionIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;

		OnClientLeave(freeSessionId);

		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);

		CNetSession::Free(pSession);
		InterlockedDecrement(&m_monitoringTargets->sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	void CNetServer::FristPostAcceptEx() noexcept
	{
		// ó���� AcceptEx�� �ɾ�ΰ� ����
		for (int i = 0; i < ACCEPTEX_COUNT; i++)
		{
			PostAcceptEx(i);
		}
	}

	BOOL CNetServer::PostAcceptEx(INT index) noexcept
	{
		int retVal;
		int errVal;

		CNetSession *newAcceptEx = CNetSession::Alloc();
		m_arrAcceptExSessions[index] = newAcceptEx;

		newAcceptEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (newAcceptEx->m_sSessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
			return FALSE;
		}

		ZeroMemory(newAcceptEx->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));
		CNetSession::s_OverlappedPool.SetAcceptExIndex((ULONG_PTR)newAcceptEx->m_pMyOverlappedStartAddr, index);

		retVal = m_lpfnAcceptEx(m_sListenSocket, newAcceptEx->m_sSessionSocket
			, newAcceptEx->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, newAcceptEx->m_pMyOverlappedStartAddr);
		if (retVal == FALSE)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CNetServer::AcceptExCompleted(CNetSession *pSession) noexcept
	{
		InterlockedIncrement(&m_monitoringTargets->acceptTPS);

		int retVal;
		int errVal;
		retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&m_sListenSocket, sizeof(SOCKET));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();

			if (!m_isStop)
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) ���� : %d", errVal);
			return FALSE;
		}

		// ������ ���Ͽ� ���� IOCP ���
		CreateIoCompletionPort((HANDLE)pSession->m_sSessionSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);

		SOCKADDR_IN *localAddr = nullptr;
		INT localAddrLen;

		SOCKADDR_IN *remoteAddr = nullptr;
		INT remoteAddrLen;
		m_lpfnGetAcceptExSockaddrs(pSession->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (SOCKADDR **)&localAddr, &localAddrLen, (SOCKADDR **)&remoteAddr, &remoteAddrLen);

		InetNtop(AF_INET, &remoteAddr->sin_addr, pSession->m_ClientAddrBuffer, 16);

		pSession->m_ClientPort = remoteAddr->sin_port;

		// TODO - ������ ��� ���
		if (!OnConnectionRequest(pSession->m_ClientAddrBuffer, pSession->m_ClientPort))
			return FALSE;

		USHORT index;
		// ���� ���� : FALSE
		if (!m_stackDisconnectIndex.Pop(&index))
		{
			MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
			return FALSE;
		}

		UINT64 combineId = NetworkLib::Core::Utils::CombineSessionIndex(index, InterlockedIncrement64(&m_iCurrentID));

		pSession->Init(combineId);

		InterlockedIncrement(&m_monitoringTargets->sessionCount);
		m_arrPSessions[index] = pSession;

		// ���� �ߴ� ���¸� ���� ����
		if (m_isStop)
		{
			Disconnect(combineId);
			return FALSE;
		}

		return TRUE;
	}

	int CNetServer::WorkerThread() noexcept
	{
		int retVal;
		DWORD flag = 0;
		while (m_bIsWorkerRun)
		{
			DWORD dwTransferred = 0;
			CNetSession *pSession = nullptr;
			OVERLAPPED *lpOverlapped = nullptr;

			retVal = GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
				, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
				, INFINITE);


			NetworkLib::DataStructures::IOOperation oper;
			if ((UINT64)lpOverlapped >= 3)
			{
				if ((UINT64)lpOverlapped < 0xFFFF)
				{
					oper = (NetworkLib::DataStructures::IOOperation)(UINT64)lpOverlapped;
				}
				else
				{
					oper = CNetSession::s_OverlappedPool.CalOperation((ULONG_PTR)lpOverlapped);
				}
			}

			if (lpOverlapped == nullptr)
			{
				if (retVal == FALSE)
				{
					// GetQueuedCompletionStatus Fail
					break;
				}

				if (dwTransferred == 0 && pSession == 0)
				{
					// ���� ���� ��ƾ
					PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
					break;
				}
			}
			// ���� ���� ����
			else if (dwTransferred == 0 && oper != NetworkLib::DataStructures::IOOperation::ACCEPTEX && (UINT)oper < 3)
			{
				// Disconnect(pSession->m_uiSessionID);
			}
			else
			{
				switch (oper)
				{
				case NetworkLib::DataStructures::IOOperation::ACCEPTEX:
				{
					// Accept�� ������ ���� �����͸� ����
					INT index = CNetSession::s_OverlappedPool.GetAcceptExIndex((ULONG_PTR)lpOverlapped);
					pSession = m_arrAcceptExSessions[index];

					// �̰� �����ϸ� ���� ����
					// * ���� ������ ��Ȳ - setsockopt, OnConnectionRequest
					// * ioCount ������ 1�� ����
					// * �ٷ� ��� ����
					// * �ٸ� I/O ��û�� �Ȱɰ� ������ ������ �Ʒ��� ioCount 0�� ������ ���� ������ ����
					InterlockedIncrement(&pSession->m_iIOCountAndRelease);
					if (!AcceptExCompleted(pSession))
					{
						// ������ �ε����� ���� ������ �ٽ� �ɾ���
						if (!m_isStop)
							PostAcceptEx(index);

						InterlockedDecrement(&pSession->m_iIOCountAndRelease);
						closesocket(pSession->m_sSessionSocket);
						CNetSession::Free(pSession);
						continue;
					}

					// OnAccept ó���� ���⼭
					OnAccept(pSession->m_uiSessionID);
					// �ش� ���ǿ� ���� Recv ����
					pSession->PostRecv();

					if (!m_isStop)
						PostAcceptEx(index);

					break;
				}
				break;
				case NetworkLib::DataStructures::IOOperation::RECV:
				{
					pSession->RecvCompleted(dwTransferred);
					pSession->PostRecv();
				}
				break;
				case NetworkLib::DataStructures::IOOperation::SEND:
				{
					pSession->SendCompleted(dwTransferred);
				}
				break;
				case NetworkLib::DataStructures::IOOperation::SENDPOST:
				{
					pSession->PostSend();
					continue;
				}
				break;
				case NetworkLib::DataStructures::IOOperation::RELEASE_SESSION:
				{
					ReleaseSessionPQCS(pSession);
					continue;
				}
				break;
				}
			}

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
		}

		return 0;
	}

	void CNetServer::RegisterSystemTimerEvent()
	{
		// MonitorTimerEvent *pMonitorEvent = new MonitorTimerEvent;
		// pMonitorEvent->SetEvent();
		// CContentsThread::EnqueueEvent(pMonitorEvent);
		// CContentThread::s_arrContentThreads[2]->EnqueueEventMy(pMonitorEvent);

		// KeyBoardTimerEvent *pKeyBoardEvent = new KeyBoardTimerEvent;
		// pKeyBoardEvent->SetEvent();
		// CContentsThread::EnqueueEvent(pKeyBoardEvent);
		// CContentThread::s_arrContentThreads[2]->EnqueueEventMy(pKeyBoardEvent);

		// SendAllTimerEvent *pSendAllEvent = new SendAllTimerEvent;
		// pSendAllEvent->SetEvent();
		// CContentThread::s_arrContentThreads[3]->EnqueueEventMy(pSendAllEvent);
	}
}