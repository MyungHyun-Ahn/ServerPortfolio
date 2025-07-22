#pragma once

#include "CTLSMemoryPool.h"

namespace MHLib::memory
{
	using namespace MHLib::containers;

	// WSASend, WSARecv ������ �� ����ȭ ����
	// Byte ����� ���� Bucket�� ũ�Ⱑ ������
	// ex 4KB -> BucketSize = 64 * 1024 / 4 * 1024 = 16
	template<int sizeByte = 4 * 1024>
	struct TLSPagePoolNode
	{
		TLSPagePoolNode() = default;

		void *ptr;
		TLSPagePoolNode *next;
	};

	template<int sizeByte = 4 * 1024, int bucketCount = 2>
	struct PageBucket
	{
		static_assert((64 * 1024) % sizeByte == 0);
		static_assert(sizeByte >= 64);

		// ��Ŷ ������ ���
		static constexpr int BUCKET_SIZE = (64 * 1024) / sizeByte;
		static constexpr int TLS_BUCKET_COUNT = bucketCount;


		~PageBucket() noexcept
		{

		}

		static PageBucket *GetTLSPageBucket() noexcept
		{
			thread_local PageBucket buckets[TLS_BUCKET_COUNT];
			PageBucket *pPtr = buckets;
			return pPtr;
		}

		static PageBucket *GetFreePageBucket() noexcept
		{
			thread_local PageBucket freeBucket;
			return &freeBucket;
		}

		void Clear() noexcept
		{
			m_pTop = nullptr;
			m_iSize = 0;
		}

		int Push(TLSPagePoolNode<sizeByte> *freeNode) noexcept
		{
			freeNode->next = m_pTop;
			m_pTop = freeNode;
			m_iSize++;
			return m_iSize;
		}

		TLSPagePoolNode<sizeByte> *Pop() noexcept
		{
			if (m_iSize == 0)
				return nullptr;

			TLSPagePoolNode<sizeByte> *ret = m_pTop;
			m_pTop = m_pTop->next;
			m_iSize--;
			return ret;
		}

		TLSPagePoolNode<sizeByte> *m_pTop = nullptr;
		int m_iSize = 0;
	};

	template<int sizeByte = 4 * 1024, int bucketCount = 2, int pageLock = false>
	class CTLSPagePoolManager;

	template<int sizeByte = 4 * 1024, int bucketCount = 2, bool pageLock = false>
	class CTLSSharedPagePool;


	template<int sizeByte = 4 * 1024, int bucketCount = 2, bool pageLock = false>
	class CTLSPagePool
	{
	public:
		friend class CTLSPagePoolManager<sizeByte, bucketCount, pageLock>;

	private:
		CTLSPagePool(CTLSSharedPagePool<sizeByte, bucketCount, pageLock> *sharedPool) noexcept : m_pTLSSharedPagePool(sharedPool)
		{
			m_MyBucket = PageBucket<sizeByte, bucketCount>::GetTLSPageBucket();
			m_FreeBucket = PageBucket<sizeByte, bucketCount>::GetFreePageBucket();
		}

		~CTLSPagePool() noexcept
		{

		}

		void *Alloc() noexcept
		{
			// ���� �� ��Ȳ
			if (m_iCurrentBucketIndex == PageBucket<sizeByte, bucketCount>::TLS_BUCKET_COUNT)
			{
				m_iCurrentBucketIndex--;
			}
			else if (m_iCurrentBucketIndex != -1 && m_MyBucket[m_iCurrentBucketIndex].m_iSize == 0)
			{
				m_iCurrentBucketIndex--;
			}

			if (m_iCurrentBucketIndex == -1)
			{
				m_iCurrentBucketIndex++;
				m_MyBucket[m_iCurrentBucketIndex].m_pTop = m_pTLSSharedPagePool->Alloc();
				m_MyBucket[m_iCurrentBucketIndex].m_iSize = PageBucket<sizeByte, bucketCount>::BUCKET_SIZE;
			}

			TLSPagePoolNode<sizeByte> *retNode = m_MyBucket[m_iCurrentBucketIndex].Pop();
			void *ret = retNode->ptr;
			m_pTLSSharedPagePool->s_NodePool.Free(retNode);
			return ret;
		}

		void Free(void *delPtr) noexcept
		{
			TLSPagePoolNode<sizeByte> *delNode = m_pTLSSharedPagePool->s_NodePool.Alloc();
			delNode->ptr = delPtr;
			delNode->next = nullptr;

			if (m_iCurrentBucketIndex == PageBucket<sizeByte, bucketCount>::TLS_BUCKET_COUNT)
			{
				// Free ��Ŷ����
				int retSize = m_FreeBucket->Push(delNode);
				if (retSize == PageBucket<sizeByte, bucketCount>::BUCKET_SIZE)
				{
					// ���� Ǯ�� ��ȯ
					m_pTLSSharedPagePool->Free(m_FreeBucket->m_pTop);
					m_FreeBucket->Clear();
				}
			}
			else
			{
				if (m_iCurrentBucketIndex != -1)
				{
					int retSize = m_MyBucket[m_iCurrentBucketIndex].Push(delNode);
					if (retSize == PageBucket<sizeByte, bucketCount>::BUCKET_SIZE)
					{
						m_iCurrentBucketIndex++;
					}
				}
				else
				{
					m_iCurrentBucketIndex++;
					m_MyBucket[m_iCurrentBucketIndex].Push(delNode);
				}
			}
		}

	public:
		static constexpr int MyBucketCount = 2;

	private:

		PageBucket<sizeByte, bucketCount> *m_MyBucket;
		int m_iCurrentBucketIndex = -1;

		// ���� MyBucket�� ���� �� ��� -> FreeBucket�� ��ȯ
		// FreeBucket�� ���� �� ��� ���� Bucket Ǯ�� ��ȯ
		PageBucket<sizeByte, bucketCount> *m_FreeBucket;

		LONG	m_lUsedCount = 0;

		CTLSSharedPagePool<sizeByte, bucketCount, pageLock> *m_pTLSSharedPagePool = nullptr;
	};

	template<int sizeByte, int bucketCount, int pageLock>
	class CTLSPagePoolManager
	{
	public:
		friend class CTLSPagePool<sizeByte, bucketCount, pageLock>;

		CTLSPagePoolManager() noexcept
		{
			m_dwTLSPagePoolIdx = TlsAlloc();
			if (m_dwTLSPagePoolIdx == TLS_OUT_OF_INDEXES)
			{
				DWORD err = GetLastError();
				__debugbreak();
			}
		}

		~CTLSPagePoolManager() noexcept
		{
			// ���� �۾�
		}

		LONG AllocTLSPagePoolIdx() noexcept
		{
			LONG idx = InterlockedIncrement(&m_iTLSPagePoolsCurrentSize);
			return idx;
		}

		void *Alloc() noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSPagePoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTLSPagePoolIdx();
				TlsSetValue(m_dwTLSPagePoolIdx, (LPVOID)tlsIndex);
				m_arrTLSPagePools[tlsIndex] = new CTLSPagePool<sizeByte, bucketCount, pageLock>(&m_TLSSharedPagePool);
			}
			void *ptr = m_arrTLSPagePools[tlsIndex]->Alloc();
			return ptr;
		}

		void Free(void *freePtr) noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSPagePoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTLSPagePoolIdx();
				TlsSetValue(m_dwTLSPagePoolIdx, (LPVOID)tlsIndex);
				m_arrTLSPagePools[tlsIndex] = new CTLSPagePool<sizeByte, bucketCount, pageLock>(&m_TLSSharedPagePool);
			}

			m_arrTLSPagePools[tlsIndex]->Free(freePtr);
		}

		LONG GetCapacity() noexcept
		{
			return m_TLSSharedPagePool.GetCapacity();
		}

		LONG GetUseCount() const noexcept
		{

		}

	private:
		static constexpr int m_iThreadCount = 50;

		DWORD			m_dwTLSPagePoolIdx;

		// �ε��� 0���� TLS�� �ʱⰪ���� ����� �Ұ���
		CTLSPagePool<sizeByte, bucketCount, pageLock> *m_arrTLSPagePools[m_iThreadCount];
		LONG					m_iTLSPagePoolsCurrentSize = 0;

		CTLSSharedPagePool<sizeByte, bucketCount, pageLock> m_TLSSharedPagePool = CTLSSharedPagePool<sizeByte, bucketCount, pageLock>();
	};

	template<int sizeByte, int bucketCount, bool pageLock>
	class CTLSSharedPagePool
	{
	public:
		friend class CTLSPagePool<sizeByte, bucketCount, pageLock>;

		using BucketNode = TLSMemoryPoolNode<PageBucket<sizeByte, bucketCount>>;
		using Node = TLSPagePoolNode<sizeByte>;

		CTLSSharedPagePool() noexcept
		{
			m_NULL = InterlockedIncrement(&s_iQueueIdentifier) % 0xFFFF;
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			BucketNode *pHead = s_BucketPool.Alloc();
			pHead->next = (BucketNode *)m_NULL;
			m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_pTail = m_pHead;
		}

		Node *Alloc() noexcept
		{
			// Free ������ Bucket�� �ִ��� ���� üũ
			if (InterlockedDecrement(&m_iUseCount) < 0)
			{
				InterlockedIncrement(&m_iUseCount);

				// ������ �� �Ҵ�
				Node *retTop = CreateNodeList();
				InterlockedAdd(&m_iCapacity, 64); // 64KB ������ ����
				return retTop;
			}

			Node *retTop;

			while (true)
			{
				ULONG_PTR readHead = m_pHead;
				ULONG_PTR readTail = m_pTail;
				BucketNode *readHeadAddr = (BucketNode *)GetAddress(readHead);
				ULONG_PTR next = (ULONG_PTR)readHeadAddr->next;
				BucketNode *nextAddr = (BucketNode *)GetAddress(next);

				// Head�� Tail�� ���ٸ� �ѹ� �о��ְ� Dequeue ����
				if (readHead == readTail)
				{
					BucketNode *readTailAddr = (BucketNode *)GetAddress(readTail);
					ULONG_PTR readTailNext = (ULONG_PTR)readTailAddr->next;
					InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
				}

				// Head->next NULL�� ���� ť�� ����� �� ��
				if (next == m_NULL)
				{
					continue;
				}
				else
				{

					if (next < 0xFFFF)
					{
						continue;
					}

					retTop = nextAddr->data.m_pTop;
					if (InterlockedCompareExchange(&m_pHead, next, readHead) == readHead)
					{
						BucketNode *readHeadAddr = (BucketNode *)GetAddress(readHead);
						s_BucketPool.Free(readHeadAddr);
						break;
					}
				}
			}

			return retTop;
		}

		// Enqueue
		void Free(TLSPagePoolNode<sizeByte> *freePtr) noexcept
		{
			// �ݳ�
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);

			BucketNode *newNode = s_BucketPool.Alloc();
			newNode->data.m_pTop = freePtr;
			newNode->data.m_iSize = PageBucket<sizeByte, bucketCount>::BUCKET_SIZE;
			newNode->next = (BucketNode *)m_NULL;

			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

			while (true)
			{
				ULONG_PTR readTail = m_pTail;
				Node *readTailAddr = (Node *)GetAddress(readTail);
				ULONG_PTR next = (ULONG_PTR)readTailAddr->next;

				if (next != m_NULL)
				{
					InterlockedCompareExchange(&m_pTail, next, readTail);
					continue;
				}

				if (InterlockedCompareExchange((ULONG_PTR *)&readTailAddr->next, combinedNode, m_NULL) == m_NULL)
				{
					InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
					break;
				}
			}

			InterlockedIncrement(&m_iUseCount);
		}

	public:
		LONG GetCapacity() const noexcept
		{
			return m_iCapacity;
		}

	private:
		// ��Ŷ ������ŭ VirtualAlloc �Ҵ��ؼ� �����ֱ�
		Node *CreateNodeList() noexcept
		{
			// 64KB �Ҵ� ����
			BYTE *ptr = (BYTE *)VirtualAlloc(nullptr, 64 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			if constexpr (pageLock)
			{
				int err;
				if (VirtualLock(ptr, 64 * 1024) == FALSE)
				{
					err = GetLastError();
					__debugbreak();
				}
			}

			Node *pTop = nullptr;
			for (int i = 0; i < PageBucket<sizeByte, bucketCount>::BUCKET_SIZE; i++)
			{
				Node *node = s_NodePool.Alloc();
				node->next = pTop;
				node->ptr = (BYTE *)ptr + i * sizeByte;
				pTop = node;
			}

			return pTop;
		}

	private:
		ULONG_PTR			m_pHead = NULL;
		ULONG_PTR			m_pTail = NULL;
		ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
		LONG				m_iUseCount = 0;
		LONG				m_iCapacity = 0;
		ULONG_PTR			m_NULL; // NULL üũ��

		inline static LONG	s_iQueueIdentifier = 0;
		inline static CTLSMemoryPoolManager<BucketNode, 64, 2> s_BucketPool = CTLSMemoryPoolManager<BucketNode, 64, 2>();
		inline static CTLSMemoryPoolManager<Node, 64, 2> s_NodePool = CTLSMemoryPoolManager<Node, 64, 2>();
	};
}