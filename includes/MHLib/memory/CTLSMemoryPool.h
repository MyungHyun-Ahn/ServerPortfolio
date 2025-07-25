#pragma once

#include "CLFMemoryPool.h"

#define DECLARE_ALLOC_FREE(Type, PoolName)														\
	public:																						\
		inline static Type *Alloc() noexcept													\
		{																						\
			Type *newObject = PoolName.Alloc();													\
			return newObject;																	\
		}																						\
																								\
																								\
		inline static void Free(Type *delObject) noexcept										\
		{																						\
			PoolName.Free(delObject);															\
		}																						\
	private:

#define DECLARE_ALLOC_FREE_WITH_INIT(Type, PoolName, InitFunc)														\
	public:																						\
		inline static Type *Alloc() noexcept													\
		{																						\
			Type *newObject = PoolName.Alloc();													\
			newObject->InitFunc();																\
			return newObject;																	\
		}																						\
																								\
																								\
		inline static void Free(Type *delObject) noexcept										\
		{																						\
			PoolName.Free(delObject);															\
		}																						\
	private:

#define DECLARE_GET_POOL_INFO(Type, PoolName)													\
	public:																						\
		inline static LONG GetPoolCapacity() noexcept { return PoolName.GetCapacity(); }		\
		inline static LONG GetPoolUsage() noexcept { return PoolName.GetUseCount(); }			\
	private:


#define USE_TLS_POOL(Type, PoolName)															\
	DECLARE_ALLOC_FREE(Type, PoolName)															\
	DECLARE_GET_POOL_INFO(Type, PoolName)														\
	inline static MHLib::memory::CTLSMemoryPoolManager<Type> PoolName;

#define USE_TLS_POOL_WITH_INIT(Type, PoolName, InitFunc)										\
	DECLARE_ALLOC_FREE_WITH_INIT(Type, PoolName, InitFunc)										\
	DECLARE_GET_POOL_INFO(Type, PoolName)														\
	inline static MHLib::memory::CTLSMemoryPoolManager<Type> PoolName;

namespace MHLib::memory
{
	using namespace MHLib::containers;

	template<typename DATA>
	struct TLSMemoryPoolNode
	{
		TLSMemoryPoolNode() = default;

		DATA data;
		TLSMemoryPoolNode<DATA> *next;
	};

	template<typename DATA, int bucketSize = 64, int bucketCount = 2>
	struct Bucket
	{
		static constexpr int BUCKET_SIZE = bucketSize;
		static constexpr int TLS_BUCKET_COUNT = bucketCount;

		~Bucket() noexcept
		{
			// 누수 감지 코드 등 넣어도 됨
		}

		static Bucket *GetTLSBucket() noexcept
		{
			thread_local Bucket buckets[TLS_BUCKET_COUNT];
			Bucket *pPtr = buckets;
			return pPtr;
		}

		static Bucket *GetFreeBucket() noexcept
		{
			thread_local Bucket freeBucket;
			return &freeBucket;
		}



		inline void Clear() noexcept
		{
			m_pTop = nullptr;
			m_iSize = 0;
		}

		inline int Push(TLSMemoryPoolNode<DATA> *freeNode) noexcept
		{
			// Top 갱신
			freeNode->next = m_pTop;
			m_pTop = freeNode;
			m_iSize++;
			return m_iSize;
		}

		inline TLSMemoryPoolNode<DATA> *Pop() noexcept
		{
			if (m_iSize == 0)
				return nullptr;

			TLSMemoryPoolNode<DATA> *ret = m_pTop;
			m_pTop = m_pTop->next;
			m_iSize--;

			return ret;
		}

		// 사실상 스택 역할
		TLSMemoryPoolNode<DATA> *m_pTop = nullptr;
		int m_iSize = 0;
	};

	// 기본 값은 큐 쓰도록
	// 테스트 결과 큐가 조금 더 괜찮았음
	template<typename DATA, int bucketSize = 64, int bucketCount = 2, bool useQueue = TRUE>
	class CTLSSharedMemoryPool
	{

	};

	// 버킷 풀, 사실 상 스택
	// Capacity 를 공용 풀에서만 측정해도 될 듯
	template<typename DATA, int bucketSize, int bucketCount>
	class CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, FALSE>
	{
	public:
		using Node = MemoryPoolNode<Bucket<DATA, bucketSize, bucketCount>>;

		// BUCKET_SIZE 만큼 연결된 버킷의 m_pTop을 반환
		TLSMemoryPoolNode<DATA> *Alloc() noexcept
		{
			// Free 상태인 Bucket이 있는지 먼저 체크
			if (InterlockedDecrement(&m_iUseCount) < 0)
			{
				InterlockedIncrement(&m_iUseCount);
				// 없으면 찐 할당
				TLSMemoryPoolNode<DATA> *retTop = CreateNodeList();
				TLSMemoryPoolNode<DATA> *node = retTop;

				InterlockedAdd(&m_iCapacity, Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE);
				return retTop;

			}

			ULONG_PTR readTop;
			ULONG_PTR newTop;
			Node *readTopAddr;

			do
			{
				readTop = m_pTop;
				readTopAddr = (Node *)GetAddress(readTop);
				newTop = readTopAddr->next;
			} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

			// Pop 성공
			TLSMemoryPoolNode<DATA> *retTop = readTopAddr->data.m_pTop;

			readTopAddr->data.Clear();
			s_BucketPool.Free(readTopAddr);
			return retTop;
		}

		// BUCKET_SIZE 만큼 연결된 버킷의 m_pTop을 반납
		void Free(TLSMemoryPoolNode<DATA> *freePtr) noexcept
		{
			// 반납
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			ULONG_PTR readTop;
			Node *newTop = s_BucketPool.Alloc();
			newTop->data.m_pTop = freePtr;
			newTop->data.m_iSize = Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

			do
			{
				// readTop 또한 ident와 합성된 주소
				readTop = m_pTop;
				newTop->next = readTop;
			} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);


			InterlockedIncrement(&m_iUseCount);
		}

	public:
		inline LONG GetCapacity() const noexcept
		{
			return m_iCapacity;
		}

	private:
		// 버킷 크기만큼의 노드 리스트를 생성(스택)
		inline TLSMemoryPoolNode<DATA> *CreateNodeList() noexcept
		{
			TLSMemoryPoolNode<DATA> *pTop = nullptr;
			for (int i = 0; i < Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE; i++)
			{
				TLSMemoryPoolNode<DATA> *newNode = new TLSMemoryPoolNode<DATA>();
				newNode->next = pTop;
				pTop = newNode;
			}
			return pTop;
		}

	private:
		ULONG_PTR		m_pTop = NULL; // Bucket Top
		ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG			m_iUseCount = 0;
		LONG			m_iCapacity = 0;
		inline static CLFMemoryPool<Node> s_BucketPool = CLFMemoryPool<Node>(100);
	};

	template<typename DATA, int bucketSize, int bucketCount>
	class CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, TRUE>
	{
	public:
		using Node = MemoryPoolNode<Bucket<DATA, bucketSize, bucketCount>>;

		// 생성자
		CTLSSharedMemoryPool<DATA, bucketSize, bucketCount>() noexcept
		{
			m_NULL = InterlockedIncrement(&s_iQueueIdentifier) % 0xFFFF;
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			Node *pHead = s_BucketPool.Alloc();
			pHead->next = m_NULL;
			m_pHead = CombineIdentAndAddr(ident, (ULONG_PTR)pHead);
			m_pTail = m_pHead;
		}

		// Dequeue
		TLSMemoryPoolNode<DATA> *Alloc() noexcept
		{
			// Free 상태인 Bucket이 있는지 먼저 체크
			if (InterlockedDecrement(&m_iUseCount) < 0)
			{
				InterlockedIncrement(&m_iUseCount);

				// 없으면 찐 할당
				TLSMemoryPoolNode<DATA> *retTop = CreateNodeList();
				InterlockedAdd(&m_iCapacity, Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE);
				return retTop;

			}

			TLSMemoryPoolNode<DATA> *retTop;

			while (true)
			{
				ULONG_PTR readHead = m_pHead;
				ULONG_PTR readTail = m_pTail;
				Node *readHeadAddr = (Node *)GetAddress(readHead);
				ULONG_PTR next = readHeadAddr->next;
				Node *nextAddr = (Node *)GetAddress(next);

				// Head와 Tail이 같다면 한번 밀어주고 Dequeue 수행
				if (readHead == readTail)
				{
					Node *readTailAddr = (Node *)GetAddress(readTail);
					ULONG_PTR readTailNext = readTailAddr->next;
					InterlockedCompareExchange(&m_pTail, readTailNext, readTail);
				}

				// Head->next NULL인 경우는 큐가 비었을 때 뿐
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
						readHeadAddr = (Node *)GetAddress(readHead);
						s_BucketPool.Free(readHeadAddr);
						break;
					}
				}
			}

			return retTop;
		}

		// Enqueue
		void Free(TLSMemoryPoolNode<DATA> *freePtr) noexcept
		{
			// 반납
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);

			Node *newNode = s_BucketPool.Alloc();
			newNode->data.m_pTop = freePtr;
			newNode->data.m_iSize = Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE;
			newNode->next = m_NULL;

			ULONG_PTR combinedNode = CombineIdentAndAddr(ident, (ULONG_PTR)newNode);

			while (true)
			{
				ULONG_PTR readTail = m_pTail;
				Node *readTailAddr = (Node *)GetAddress(readTail);
				ULONG_PTR next = readTailAddr->next;

				if (next != m_NULL)
				{
					InterlockedCompareExchange(&m_pTail, next, readTail);
					continue;
				}

				if (InterlockedCompareExchange(&readTailAddr->next, combinedNode, m_NULL) == m_NULL)
				{
					InterlockedCompareExchange(&m_pTail, combinedNode, readTail);
					break;
				}
			}

			InterlockedIncrement(&m_iUseCount);
		}

	public:
		inline LONG GetCapacity() const noexcept
		{
			return m_iCapacity;
		}

	private:
		// 버킷 크기만큼의 노드 리스트를 생성(스택)
		inline TLSMemoryPoolNode<DATA> *CreateNodeList() noexcept
		{
			TLSMemoryPoolNode<DATA> *pTop = nullptr;
			for (int i = 0; i < Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE; i++)
			{
				TLSMemoryPoolNode<DATA> *newNode = new TLSMemoryPoolNode<DATA>();
				newNode->next = pTop;
				pTop = newNode;
			}
			return pTop;
		}

	private:
		ULONG_PTR			m_pHead = NULL;
		ULONG_PTR			m_pTail = NULL;
		ULONG_PTR			m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
		LONG				m_iUseCount = 0;
		LONG				m_iCapacity = 0;
		ULONG_PTR			m_NULL; // NULL 체크용

		inline static LONG	s_iQueueIdentifier = 0;
		inline static CLFMemoryPool<Node> s_BucketPool = CLFMemoryPool<Node>(100);
	};

	template<typename DATA, int bucketSize = 64, int bucketCount = 2, bool UseQueue = TRUE>
	class CTLSMemoryPoolManager;

	template<typename DATA, int bucketSize = 64, int bucketCount = 2, bool UseQueue = TRUE>
	class CTLSMemoryPool
	{
	public:
		friend class CTLSMemoryPoolManager<DATA, bucketSize, bucketCount, UseQueue>;

	private:
		CTLSMemoryPool(CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue> *sharedPool) noexcept : m_pTLSSharedMemoryPool(sharedPool)
		{
			m_AllocBucket = Bucket<DATA, bucketSize, bucketCount>::GetTLSBucket();
			m_FreeBucket = Bucket<DATA, bucketSize, bucketCount>::GetFreeBucket();
		}

		~CTLSMemoryPool() noexcept
		{

		}

		DATA *Alloc() noexcept
		{
			if (m_iCurrentBucketIndex == Bucket<DATA, bucketSize, bucketCount>::TLS_BUCKET_COUNT)
			{
				m_iCurrentBucketIndex--;
			}
			else if (m_iCurrentBucketIndex != -1 && m_AllocBucket[m_iCurrentBucketIndex].m_iSize == 0)
			{
				m_iCurrentBucketIndex--;
			}

			if (m_iCurrentBucketIndex == -1)
			{
				m_iCurrentBucketIndex++;
				m_AllocBucket[m_iCurrentBucketIndex].m_pTop = m_pTLSSharedMemoryPool->Alloc();
				m_AllocBucket[m_iCurrentBucketIndex].m_iSize = Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE;
			}

			TLSMemoryPoolNode<DATA> *retNode = m_AllocBucket[m_iCurrentBucketIndex].Pop();
			m_lUsedCount++;
			return &retNode->data;
		}

		void Free(DATA *delPtr) noexcept
		{
			if (m_iCurrentBucketIndex == Bucket<DATA, bucketSize, bucketCount>::TLS_BUCKET_COUNT)
			{
				int retSize = m_FreeBucket->Push((TLSMemoryPoolNode<DATA> *)delPtr);
				if (retSize == Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE)
				{
					m_pTLSSharedMemoryPool->Free(m_FreeBucket->m_pTop);
					m_FreeBucket->Clear();
				}
			}
			else
			{
				if (m_iCurrentBucketIndex != -1)
				{
					int retSize = m_AllocBucket[m_iCurrentBucketIndex].Push((TLSMemoryPoolNode<DATA> *)delPtr);
					if (retSize == Bucket<DATA, bucketSize, bucketCount>::BUCKET_SIZE)
					{
						m_iCurrentBucketIndex++;
					}
				}
				else
				{
					m_iCurrentBucketIndex++;
					m_AllocBucket[m_iCurrentBucketIndex].Push((TLSMemoryPoolNode<DATA> *)delPtr);
				}
			}
			m_lUsedCount--;
		}


	private:

		Bucket<DATA, bucketSize, bucketCount> *m_AllocBucket;
		int m_iCurrentBucketIndex = -1;

		// 만약 MyBucket이 가득 찬 경우 -> FreeBucket에 반환
		// FreeBucket도 가득 찬 경우 공용 Bucket 풀에 반환
		Bucket<DATA, bucketSize, bucketCount> *m_FreeBucket;

		LONG	m_lUsedCount = 0;

		CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue> *m_pTLSSharedMemoryPool = nullptr;
	};

	template<typename DATA, int bucketSize, int bucketCount, bool UseQueue>
	class CTLSMemoryPoolManager
	{
	public:
		friend class CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue>;

		CTLSMemoryPoolManager() noexcept
		{
			m_dwTLSMemoryPoolIdx = TlsAlloc();
			if (m_dwTLSMemoryPoolIdx == TLS_OUT_OF_INDEXES)
			{
				DWORD err = GetLastError();
				__debugbreak();
			}
		}

		~CTLSMemoryPoolManager() noexcept
		{
			// TLS 풀 정리 작업
		}

		LONG AllocTLSMemoryPoolIdx() noexcept
		{
			LONG idx = InterlockedIncrement(&m_iTLSMemoryPoolsCurrentSize);
			return idx;
		}

		inline DATA *Alloc() noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSMemoryPoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTLSMemoryPoolIdx();
				TlsSetValue(m_dwTLSMemoryPoolIdx, (LPVOID)tlsIndex);
				m_arrTLSMemoryPools[tlsIndex] = new CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue>(&m_TLSSharedMemoryPool);
			}

			DATA *ptr = m_arrTLSMemoryPools[tlsIndex]->Alloc();
			return ptr;
		}

		inline void Free(DATA *freePtr) noexcept
		{
			__int64 tlsIndex = (__int64)TlsGetValue(m_dwTLSMemoryPoolIdx);
			if (tlsIndex == 0)
			{
				tlsIndex = AllocTLSMemoryPoolIdx();
				TlsSetValue(m_dwTLSMemoryPoolIdx, (LPVOID)tlsIndex);
				m_arrTLSMemoryPools[tlsIndex] = new CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue>(&m_TLSSharedMemoryPool);
			}

			m_arrTLSMemoryPools[tlsIndex]->Free(freePtr);
		}

		inline LONG GetCapacity() noexcept
		{
			return m_TLSSharedMemoryPool.GetCapacity();
		}

		inline LONG GetUseCount() const noexcept
		{
			LONG usedSize = 0;
			for (int i = 1; i <= m_iTLSMemoryPoolsCurrentSize; i++)
			{
				usedSize += m_arrTLSMemoryPools[i]->m_lUsedCount;
			}

			return usedSize;
		}


	private:
		static constexpr int m_iThreadCount = 1000;

		DWORD			m_dwTLSMemoryPoolIdx;

		// 인덱스 0번은 TLS의 초기값으로 사용이 불가능
		CTLSMemoryPool<DATA, bucketSize, bucketCount, UseQueue> *m_arrTLSMemoryPools[m_iThreadCount];
		LONG					m_iTLSMemoryPoolsCurrentSize = 0;

		CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue> m_TLSSharedMemoryPool = CTLSSharedMemoryPool<DATA, bucketSize, bucketCount, UseQueue>();
	};
}