#include "pch.h"
#include "CLanClient.h"

namespace NetworkLib::Core::Lan::Client
{
		CLanClientManager *g_netClientMgr;

		void CLanSession::Clear() noexcept
		{
			for (int count = 0; count < m_iSendCount; count++)
			{
				if (m_arrPSendBufs[count]->DecreaseRef() == 0)
				{
					NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Free(m_arrPSendBufs[count]);
				}
			}

			m_iSendCount = 0;

			LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
			for (int i = 0; i < useBufferSize; i++)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pBuffer;
				// ������ ��
				if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
				{
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
					// ���� �ȵǴ� ��Ȳ
					MHLib::debug::CCrashDump::Crash();
				}

				// RefCount�� ���߰� 0�̶�� ���� �� ����
				if (pBuffer->DecreaseRef() == 0)
				{
					NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Free(pBuffer);
				}
			}

			useBufferSize = m_lfSendBufferQueue.GetUseSize();
			if (useBufferSize != 0)
			{
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"LFQueue is not empty Error");
			}

			m_sSessionSocket = INVALID_SOCKET;
			m_uiSessionID = 0;
			m_RecvBuffer.Clear();
			InterlockedExchange(&m_iSendFlag, FALSE);
		}

		void CLanSession::RecvCompleted(int size) noexcept
		{
			m_RecvBuffer.MoveRear(size);
			int currentUseSize = m_RecvBuffer.GetUseSize();
			while (currentUseSize > 0)
			{
				if (currentUseSize < sizeof(Utils::Net::Header))
					break;

				Utils::Lan::Header packetHeader;
				m_RecvBuffer.Peek((char *)&packetHeader, sizeof(Utils::Net::Header));

				if (packetHeader.len > 
					(int)NetworkLib::DataStructures::CSerializableBuffer<SERVER_TYPE::LAN>::DEFINE::PACKET_MAX_SIZE - sizeof(NetworkLib::Core::Utils::Lan::Header) || packetHeader.len >= 65535 || packetHeader.len >= m_RecvBuffer.GetCapacity())
				{
					g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
					return;
				}

				if (m_RecvBuffer.GetUseSize() < sizeof(Utils::Lan::Header) + packetHeader.len)
					break;

				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *view 
					= NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Alloc();
				m_RecvBuffer.MoveFront(sizeof(NetworkLib::Core::Utils::Lan::Header));
				view->EnqueueHeader((char *)&packetHeader, sizeof(NetworkLib::Core::Utils::Lan::Header));

				m_RecvBuffer.Dequeue(view->GetContentBufferPtr(), packetHeader.len);
				view->MoveWritePos(packetHeader.len);
				view->IncreaseRef();
				// InterlockedIncrement(&g_monitor.m_lRecvTPS);
				{
					// PROFILE_BEGIN(0, "RECV_MSG ENQUEUE");
					g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->OnRecv(m_uiSessionID, view);
				}
				currentUseSize = m_RecvBuffer.GetUseSize();
			}
		}

		bool CLanSession::SendPacket(NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *message) noexcept
		{
			// ���⼭ �ö� RefCount�� SendCompleted���� ������
			// Ȥ�� ReleaseSession
			message->IncreaseRef();
			m_lfSendBufferQueue.Enqueue(message);
			InterlockedOr(&m_iSendFlag, ENQUEUE_FLAG);
			return TRUE;
		}

		void CLanSession::SendCompleted(int size) noexcept
		{
			// m_iSendCount�� �ϰ� �Ҵ� ������ ����
			// * ����ŷ I/O�� ���� Send�� ��û�� �����ͺ��� �� ������ ��Ȳ�� �߻� ����
			// * �񵿱� I/O�� ������ ���� ������ �Ϸ� ������ ������


			int count;
			for (count = 0; count < m_iSendCount; count++)
			{
				// RefCount�� ���߰� 0�̶�� ���� �� ����
				if (m_arrPSendBufs[count]->DecreaseRef() == 0)
				{
					NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN>::Free(m_arrPSendBufs[count]);
				}
			}

			m_iSendCount = 0;

			InterlockedExchange(&m_iSendFlag, FALSE);

			PostSend();
		}

		bool CLanSession::PostRecv() noexcept
		{
			int errVal;
			int retVal;

			// ���⼭�� ���� Ref ������ �ʿ�
			// * WSARecv���� Ref ������ �Ұ���

			InterlockedIncrement(&m_iIOCountAndRelease);
			if ((m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
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

		bool CLanSession::PostSend(bool isPQCS) noexcept
		{
			int errVal;
			int retVal;
			int sendUseSize;

			if (!isPQCS)
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
			if ((m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
			{
				InterlockedDecrement(&m_iIOCountAndRelease);
				InterlockedExchange(&m_iSendFlag, FALSE);
				return FALSE;
			}

			WSABUF wsaBuf[NetworkLib::Core::Utils::WSASEND_MAX_BUFFER_COUNT];

			m_iSendCount = min(sendUseSize, NetworkLib::Core::Utils::WSASEND_MAX_BUFFER_COUNT);
			// WSASEND_MAX_BUFFER_COUNT ��ŭ 1�ʿ� ��� �������� ī��Ʈ
			// �� ��ġ�� ���ٸ� �� �ø� ��
			// if (m_iSendCount == WSASEND_MAX_BUFFER_COUNT)
			// 	InterlockedIncrement(&g_monitor.m_lMaxSendCount);

			int count;
			for (count = 0; count < m_iSendCount; count++)
			{
				NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *pBuffer;
				// ������ ��
				if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
				{
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
					// ���� �ȵǴ� ��Ȳ
					MHLib::debug::CCrashDump::Crash();
				}

				wsaBuf[count].buf = pBuffer->GetBufferPtr();
				wsaBuf[count].len = pBuffer->GetFullSize();

				m_arrPSendBufs[count] = pBuffer;
			}

			InterlockedAnd(&m_iSendFlag, TRUE);

			ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));

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
					if (m_iCacelIoCalled)
					{
						CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
						return FALSE;
					}
				}
			}

			return TRUE;
		}

		// NetClient
		// ���� ���� �غ� �۾�
		BOOL CLanClient::Init(INT maxSessionCount) noexcept
		{
			m_iMaxSessionCount = maxSessionCount;
			// ��Ŀ��Ʈ ���� ä���
			USHORT val = m_iMaxSessionCount - 1;
			for (int i = 0; i < m_iMaxSessionCount; i++)
			{
				m_stackDisconnectIndex.Push(val--);
			}

			m_arrPSessions = new CLanSession * [maxSessionCount];

			return TRUE;
		}

		void CLanClient::SendPacket(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *sBuffer) noexcept
		{
			CLanSession *pSession = m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(sessionID)];

			InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			// ReleaseFlag�� �̹� ���� ��Ȳ
			if ((pSession->m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
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
				Utils::Lan::Header header;
				header.len = sBuffer->GetDataSize();
				sBuffer->EnqueueHeader((char *)&header, sizeof(Utils::Lan::Header));
			}

			pSession->SendPacket(sBuffer);
			pSession->PostSend();

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
		}

		void CLanClient::EnqueuePacket(const UINT64 sessionID, NetworkLib::DataStructures::CSerializableBuffer<NetworkLib::SERVER_TYPE::LAN> *sBuffer) noexcept
		{
			CLanSession *pSession = m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(sessionID)];

			InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			// ReleaseFlag�� �̹� ���� ��Ȳ
			if ((pSession->m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
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
				Utils::Lan::Header header;
				header.len = sBuffer->GetDataSize();
				sBuffer->EnqueueHeader((char *)&header, sizeof(Utils::Lan::Header));
			}

			pSession->SendPacket(sBuffer);

			// {
			// 	// PROFILE_BEGIN(0, "WSASend");
			// 	pSession->PostSend();
			// }

			// {
			// 	// PROFILE_BEGIN(0, "PQCS");
			// 
			// 	if (pSession->m_iSendFlag == FALSE)
			// 	{
			// 		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::SENDPOST);
			// 	}
			// }

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
		}

		BOOL CLanClient::Disconnect(const UINT64 sessionID, BOOL isPQCS) noexcept
		{
			CLanSession *pSession = m_arrPSessions[NetworkLib::Core::Utils::GetSessionIndex(sessionID)];
			if (pSession == nullptr)
				return FALSE;

			// ReleaseFlag�� �̹� ���� ��Ȳ
			InterlockedIncrement(&pSession->m_iIOCountAndRelease);
			if ((pSession->m_iIOCountAndRelease & CLanSession::RELEASE_FLAG) == CLanSession::RELEASE_FLAG)
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

		BOOL CLanClient::ReleaseSession(CLanSession *pSession, BOOL isPQCS) noexcept
		{
			// IoCount üũ�� ReleaseFlag ������ ���ÿ�
			if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CLanSession::RELEASE_FLAG, 0) != 0)
			{
				// ioCount 0�� �ƴϰų� �̹� Release ���� ���ΰ� �ִٸ� return
				return FALSE;
			}

			if (isPQCS)
			{
				PostQueuedCompletionStatus(g_netClientMgr->m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)NetworkLib::DataStructures::IOOperation::RELEASE_SESSION);
				return TRUE;
			}

			USHORT index = NetworkLib::Core::Utils::GetSessionIndex(pSession->m_uiSessionID);
			closesocket(pSession->m_sSessionSocket);
			UINT64 freeSessionId = pSession->m_uiSessionID;
			CLanSession::Free(pSession);
			InterlockedDecrement(&m_iSessionCount);

			OnDisconnect(freeSessionId);

			m_stackDisconnectIndex.Push(index);

			return TRUE;
		}

		BOOL CLanClient::ReleaseSessionPQCS(CLanSession *pSession) noexcept
		{
			USHORT index = NetworkLib::Core::Utils::GetSessionIndex(pSession->m_uiSessionID);
			closesocket(pSession->m_sSessionSocket);
			UINT64 freeSessionId = pSession->m_uiSessionID;
			CLanSession::Free(pSession);
			InterlockedDecrement(&m_iSessionCount);

			OnDisconnect(freeSessionId);

			m_stackDisconnectIndex.Push(index);

			return TRUE;
		}

		BOOL CLanClient::PostConnectEx(const CHAR *ip, const USHORT port) noexcept
		{
			int retVal;
			int errVal;

			CLanSession *pConnectSession = CLanSession::Alloc();
			pConnectSession->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (pConnectSession->m_sSessionSocket == INVALID_SOCKET)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
				return FALSE;
			}

			// �۽� ���۸� 0���� ���� �������� I/O�� �츮�� ���۸� �̿��ϵ��� ����
			if (USE_ZERO_COPY)
			{
				DWORD sendBufferSize = 0;
				retVal = setsockopt(pConnectSession->m_sSessionSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
				if (retVal == SOCKET_ERROR)
				{
					errVal = WSAGetLastError();
					MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
					return FALSE;
				}
			}

			// LINGER option ����
			LINGER ling{ 1, 0 };
			retVal = setsockopt(pConnectSession->m_sSessionSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
				return FALSE;
			}

			SOCKADDR_IN clientAddr;
			ZeroMemory(&clientAddr, sizeof(clientAddr));
			clientAddr.sin_family = AF_INET;
			clientAddr.sin_addr.s_addr = INADDR_ANY;
			clientAddr.sin_port = 0;

			retVal = bind(pConnectSession->m_sSessionSocket, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"bind() ���� : %d", errVal);
				return FALSE;
			}

			// ���� ID ����
			USHORT index;
			// ���� ���� : FALSE
			if (!m_stackDisconnectIndex.Pop(&index))
			{
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
				return FALSE;
			}

			UINT64 combineId = NetworkLib::Core::Utils::CombineSessionIndex(index, InterlockedIncrement64(&m_iCurrentID));
			pConnectSession->Init(combineId, m_ClientMgrIndex);

			m_arrPSessions[index] = pConnectSession;

			g_netClientMgr->RegisterIoCompletionPort(pConnectSession->m_sSessionSocket, (ULONG_PTR)pConnectSession);

			ZeroMemory(pConnectSession->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));

			SOCKADDR_IN serverAddr;
			ZeroMemory(&serverAddr, sizeof(serverAddr));
			serverAddr.sin_family = AF_INET;
			InetPtonA(AF_INET, ip, &serverAddr.sin_addr);
			serverAddr.sin_port = htons(port);

			if (!g_netClientMgr->ConnectEx(pConnectSession->m_sSessionSocket, (sockaddr *)&serverAddr, sizeof(serverAddr), pConnectSession->m_pMyOverlappedStartAddr))
			{
				errVal = WSAGetLastError();
				if (errVal != ERROR_IO_PENDING)
				{
					if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
						MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

					return FALSE;
				}
			}

			return TRUE;
		}

		BOOL CLanClient::ConnectExCompleted(CLanSession *pSession) noexcept
		{
			int retVal;
			int errVal;

			// SO_UPDATE_CONNECT_CONTEXT �ɼ� ����
			retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_CONNECT_CONTEXT) ���� : %d", errVal);
				return FALSE;
			}

			return TRUE;
		}

		unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
		{
			return g_netClientMgr->WorkerThread();
		}

		BOOL CLanClientManager::Init() noexcept
		{
			int retVal;
			int errVal;
			WSAData wsaData;

			retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (retVal != 0)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
				return FALSE;
			}

			m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, IOCP_ACTIVE_THREAD);
			if (m_hIOCPHandle == NULL)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
				return FALSE;
			}

			// ���� ����
			SOCKET dummySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
			if (dummySocket == INVALID_SOCKET)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSASocket() ���� : %d", errVal);
				return FALSE;
			}

			DWORD dwBytes;
			retVal = WSAIoctl(dummySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidConnectEx, sizeof(m_guidConnectEx), &m_lpfnConnectEx, sizeof(m_lpfnConnectEx), &dwBytes, NULL, NULL);
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				MHLib::utils::g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", MHLib::utils::LOG_LEVEL::ERR, L"WSAIoctl(lpfnConnectEx) ���� : %d", errVal);
				return FALSE;
			}

			closesocket(dummySocket);

			return TRUE;
		}

		BOOL CLanClientManager::Start() noexcept
		{
			int errVal;

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

			return TRUE;
		}

		BOOL CLanClientManager::Wait() noexcept
		{
			WaitForMultipleObjects(m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);
			return TRUE;
		}

		void CLanClientManager::RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionID)
		{
			CreateIoCompletionPort((HANDLE)sock, m_hIOCPHandle, sessionID, 0);
		}

		int CLanClientManager::WorkerThread() noexcept
		{
			int retVal;
			DWORD flag = 0;
			while (m_bIsWorkerRun)
			{
				DWORD dwTransferred = 0;
				CLanSession *pSession = nullptr;
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
						oper = CLanSession::s_OverlappedPool.CalOperation((ULONG_PTR)lpOverlapped);
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
						InterlockedIncrement(&pSession->m_iIOCountAndRelease);
						if (!m_arrNetClients[pSession->m_ClientMgrIndex]->ConnectExCompleted(pSession))
						{
							InterlockedDecrement(&pSession->m_iIOCountAndRelease);
							closesocket(pSession->m_sSessionSocket);
							CLanSession::Free(pSession);
							break;
						}

						m_arrNetClients[pSession->m_ClientMgrIndex]->OnConnect(pSession->m_uiSessionID);
						pSession->PostRecv();

						if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
							m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSession(pSession);

						continue;
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
						pSession->PostSend();
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
						m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSessionPQCS(pSession);
						continue;
					}
					break;
					}
				}

				if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
				{
					m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSession(pSession);
				}
			}

			return 0;
		}
}