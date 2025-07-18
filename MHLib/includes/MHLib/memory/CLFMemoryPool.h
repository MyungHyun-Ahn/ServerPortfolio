#pragma once

#include "../containers/LFDefine.h"

namespace MHLib::memory
{
	using namespace MHLib::containers;

#pragma warning(disable : 6001)
	// #define SAFE_MODE

	template<typename DATA>
	struct MemoryPoolNode
	{
#ifdef SAFE_MODE
		void *poolPtr;
#endif
		MemoryPoolNode() noexcept = default;

		DATA data;
		ULONG_PTR next;

#ifdef SAFE_MODE
		int safeWord = 0xFFFF;
#endif
	};

	// placement ���� ����
	template<typename DATA>
	class CLFMemoryPool
	{
		using Node = MemoryPoolNode<DATA>;

	public:
		// �� �κ��� �̱� �����忡�� ����
		// - �ٸ� �����尡 �����ǰ�� ���� �ʱ�ȭ�� �����ϸ� �ȵ�
		__forceinline CLFMemoryPool(int initCount = 0) noexcept
			: m_iCapacity(0)
		{
			// initCount ��ŭ FreeList �Ҵ�
			for (int i = 0; i < initCount; i++)
			{
				ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
				Node *newTop = NewAlloc();
				ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);
				newTop->next = m_pTop;
				m_pTop = combinedNewTop;
			}
		}

		// �ٸ� �����尡 ��� ������ ���Ŀ� �����
		__forceinline ~CLFMemoryPool() noexcept
		{
			// FreeList�� �ִ� �� ����
			while (m_pTop != NULL)
			{
				Node *delNode = (Node *)GetAddress(m_pTop);
				m_pTop = delNode->next;
				free(delNode);
			}

		}

		// Pop �ൿ
		DATA *Alloc() noexcept
		{
			Node *node;

			// nullptr�� ���� ������ �׳� �Ҵ��ϰ� ��ȯ
			// - �� ������ Free ��尡 ���ܵ� �׳� �Ҵ��ص� ��
			if (m_pTop == NULL)
			{
				node = NewAlloc();
			}
			else
			{
				ULONG_PTR readTop;
				ULONG_PTR newTop;

				do
				{
					readTop = m_pTop;
					if (readTop == NULL)
					{
						node = NewAlloc();
						break;
					}

					node = (Node *)GetAddress(readTop);
					newTop = node->next;
				} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);
			}

			InterlockedIncrement(&m_iUseCount);

			// data�� �ּҸ� ��ȯ
			return &node->data;
		}

		// �������� ġ�� Push �ൿ
		void Free(DATA *ptr) noexcept
		{
#ifdef SAFE_MODE
			unsigned __int64 intPtr = (__int64)ptr;
			intPtr -= sizeof(void *);
			Node *newTop = (Node *)intPtr;

			if (newTop->poolPtr != this)
				throw;

			if (newTop->safeWord != 0xFFFF)
				throw;
#else
			Node *newTop = (Node *)ptr;
#endif
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			ULONG_PTR readTop;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

			do
			{
				readTop = m_pTop;
				newTop->next = readTop;

			} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

			// Push ����
			InterlockedDecrement(&m_iUseCount);
		}

	private:
		__forceinline Node *NewAlloc() noexcept
		{
			Node *newNode = new Node;
#ifdef SAFE_MODE
			newNode->poolPtr = this;
#endif
			InterlockedIncrement(&m_iCapacity);
			return newNode;
		}

	public:
		inline LONG GetCapacity() const noexcept { return m_iCapacity; }
		inline LONG GetUseCount() const noexcept { return m_iUseCount; }

	private:
		LONG		m_iUseCount = 0;
		LONG		m_iCapacity = 0;
		ULONG_PTR	m_ullCurrentIdentifier = 0;
		ULONG_PTR	m_pTop = 0;
	};
}