#pragma once

#include "../memory/CTLSMemoryPool.h"

namespace MHLib::containers
{
	using namespace MHLib::memory;

	struct alignas(16) QUEUE_NODE_PTR
	{
		ULONG_PTR ptr;
		ULONG_PTR queueIdent;
	};

	template<typename DATA, bool CAS2First = TRUE>
	struct QueueNode
	{

	};

	template<typename DATA>
	struct QueueNode<DATA, TRUE>
	{
		DATA data;
		ULONG_PTR next;
	};

	template<typename DATA>
	struct QueueNode<DATA, FALSE>
	{
		DATA data;
		ULONG_PTR next;
	};

	template<FundamentalOrPointer T, bool CAS2First = FALSE>
	class CLFQueue
	{
	};

	// CAS2�� ���� �����ϴ¹���
	template<FundamentalOrPointer T>
	class CLFQueue<T, TRUE>
	{
	public:
		using Node = QueueNode<T>;

		CLFQueue() noexcept
			: m_iSize(0)
		{
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *pHead = m_QueueNodePool.Alloc();
			pHead->next = NULL;
			m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_pTail = m_pHead;
		}

		void Enqueue(T t) noexcept
		{
			// PROFILE_BEGIN(1, "Enqueue");

			UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *newNode = m_QueueNodePool.Alloc();
			newNode->data = t;
			newNode->next = NULL;
			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

			while (true)
			{
				ULONG_PTR readTail = m_pTail;
				Node *readTailAddr = (Node *)GetAddress(readTail);
				ULONG_PTR next = readTailAddr->next;

				// �ذ� 2
				// - Enqueue ��ܺ�

				// readTail�� ���ٸ� Tail�� ��ü

				if (InterlockedCompareExchange(&m_pTail, combinedNode, readTail) == readTail)
				{
					if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, NULL) == NULL)
					{
						break;
					}
					else
					{
						__debugbreak();
					}
				}
			}

			// Enqueue ����
			InterlockedIncrement(&m_iSize);
		}

		bool Dequeue(T *t) noexcept
		{
			// 2�� �ۿ� Dequeue ���ϴ� ��Ȳ
			// -1 �ߴµ� 0�� �ִ� ��
			if (InterlockedDecrement(&m_iSize) < 0)
			{
				InterlockedIncrement(&m_iSize);
				return false;
			}

			while (true)
			{
				ULONG_PTR readHead = m_pHead;
				Node *readHeadAddr = (Node *)GetAddress(readHead);
				ULONG_PTR next = readHeadAddr->next;
				Node *nextAddr = (Node *)GetAddress(next);

				// Head->next NULL�� ���� ť�� ����� �� ��
				if (next == NULL)
				{
					continue;
				}
				else
				{
					// readHead == m_pHead �� m_pHead = next
					*t = nextAddr->data;
					if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
					{
						// ���⼭ ������ ����� ������?

						Node *readHeadAddr = (Node *)GetAddress(readHead);
						m_QueueNodePool.Free(readHeadAddr);
						break;
					}
				}
			}

			return true;
		}

		inline LONG GetUseSize() const noexcept { return m_iSize; }

	private:
		ULONG_PTR			m_pHead = NULL;
		ULONG_PTR			m_pTail = NULL;
		ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
		inline static CTLSMemoryPoolManager<Node, 256, 4> m_QueueNodePool = CTLSMemoryPoolManager<Node, 256, 4>();
		LONG				m_iSize = 0;
	};

	// CAS1�� ���� �����ϴ� ����
	template<FundamentalOrPointer T>
	class CLFQueue<T, FALSE>
	{
	public:
		using Node = QueueNode<T>;

		CLFQueue() noexcept
			: m_iSize(0)
		{
			// ť �ĺ��ڷ� ���� �ٸ� NULL ���� ���
			m_NULL = InterlockedIncrement(&s_iQueueIdentifier) % 0xFFFF; // �������� ������ ��ȣ ������ŭ�� ���

			UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *pHead = m_QueueNodePool.Alloc();
			pHead->next = m_NULL;
			m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_pTail = m_pHead;
		}

		void Enqueue(T t) noexcept
		{
			UINT_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *newNode = m_QueueNodePool.Alloc();
			newNode->data = t;
			newNode->next = m_NULL;
			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

			while (true)
			{
				ULONG_PTR readTail = m_pTail;
				Node *readTailAddr = (Node *)GetAddress(readTail);
				ULONG_PTR next = readTailAddr->next;
				UINT_PTR readEnqueueId = m_ENQUEUE_ID;

				// ���� ���� ������ �ذ��ϱ� ����
				// �ᱹ Cas1�� Cas2�� ���� ���� ����
				if (readEnqueueId != m_ENQUEUE_ID)
					continue;

				if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, m_NULL) == m_NULL)
				{
					InterlockedIncrement(&m_ENQUEUE_ID);
					InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
					break; // ������� �ߴٸ� break;
				}
			}

			// Enqueue ����
			InterlockedIncrement(&m_iSize);
		}

		bool Dequeue(T *t) noexcept
		{
			// 2�� �ۿ� Dequeue ���ϴ� ��Ȳ
			// -1 �ߴµ� 0�� �ִ� ��
			if (InterlockedDecrement(&m_iSize) < 0)
			{
				InterlockedIncrement(&m_iSize);
				return false;
			}

			// PROFILE_BEGIN(1, "Dequeue");

			while (true)
			{
				ULONG_PTR readHead = m_pHead;
				Node *readHeadAddr = (Node *)GetAddress(readHead);
				ULONG_PTR next = readHeadAddr->next;
				Node *nextAddr = (Node *)GetAddress(next);

				// Head->next NULL�� ���� ť�� ����� �� ��
				if (next == m_NULL)
				{
					continue;
				}
				else
				{
					// next�� 0xFFFF���� ������ �߸� ���� ����
					if (next < 0xFFFF)
					{
						continue;
					}

					// ���� �а� CAS ����
					*t = nextAddr->data;
					if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
					{
						Node *readHeadAddr = (Node *)GetAddress(readHead);
						m_QueueNodePool.Free(readHeadAddr);
						break;
					}
				}
			}

			return true;
		}

		inline LONG GetUseSize() const noexcept { return m_iSize; }

	private:
		ULONG_PTR			m_pHead = NULL;
		
		alignas(64)
		ULONG_PTR			m_pTail = NULL;
		ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
		inline static CTLSMemoryPoolManager<Node, 256, 4> m_QueueNodePool = CTLSMemoryPoolManager<Node, 256, 4>();
		LONG				m_iSize = 0;

		ULONG_PTR			m_NULL;
		alignas(64)
		ULONG_PTR			m_ENQUEUE_ID = 0;

		inline static LONG	s_iQueueIdentifier = 0;
	};
}