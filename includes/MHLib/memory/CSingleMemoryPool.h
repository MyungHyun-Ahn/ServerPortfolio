#pragma once
// #define SAFE_MODE // �޸� Ǯ���� �Ҵ��� ������ �Ǻ� �뵵

#include <windows.h>

namespace MHLib::memory
{
	template<typename DATA>
	class CSingleMemoryPool
	{
	public:
		struct Node
		{
#ifdef SAFE_MODE
			void *poolPtr;
#endif
			DATA data;
			Node *link;

#ifdef SAFE_MODE
			int safeWord = 0xFFFF;
#endif
		};

	public:
		__forceinline CSingleMemoryPool(int initCount = 0) noexcept
			: m_iUseCount(0), m_iCapacity(0)
		{
			// initCount ��ŭ FreeList �Ҵ�
			for (int i = 0; i < initCount; i++)
			{
				Node *node = NewAlloc();
				node->link = top;
				top = node;
			}
		}

		__forceinline DATA *Alloc() noexcept
		{
			Node *node;

			if (top == nullptr)
			{
				node = NewAlloc();
			}
			else
			{
				node = top;
				top = top->link;
			}

			m_iUseCount++;
			// data�� �ּҸ� ��ȯ
			return &node->data;
		}
		__forceinline void Free(DATA *ptr) noexcept
		{
#ifdef SAFE_MODE
			unsigned __int64 intPtr = (__int64)ptr;
			intPtr -= sizeof(void *);
			Node *nodePtr = (Node *)intPtr;

			if (nodePtr->poolPtr != this)
				throw;

			if (nodePtr->safeWord != 0xFFFF)
				throw;
#else
			Node *nodePtr = (Node *)ptr;
#endif
			nodePtr->link = top;
			top = nodePtr;

			m_iUseCount--;
		}

		__forceinline LONG GetCapacity() noexcept { return m_iCapacity; }
		__forceinline LONG GetUseCount() noexcept { return m_iUseCount; }

	private:
		__forceinline Node *NewAlloc() noexcept
		{
			Node *newNode = new Node;
#ifdef SAFE_MODE
			newNode->poolPtr = this;
#endif
			m_iCapacity++;
			return newNode;
		}

	private:
		// FreeList
		int m_iUseCount = 0;
		int m_iCapacity = 0;
		Node *top = nullptr;
	};
}