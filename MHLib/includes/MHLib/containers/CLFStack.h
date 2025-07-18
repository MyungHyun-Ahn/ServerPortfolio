#pragma once

#include "../memory/CTLSMemoryPool.h"

namespace MHLib::containers
{
	using namespace MHLib::memory;

	template<typename Data>
	struct StackNode
	{
		Data data;
		ULONG_PTR next;
	};

	template<FundamentalOrPointer T, bool UseMemoryPool = TRUE>
	class CLFStack
	{
	};

	template<FundamentalOrPointer T>
	class CLFStack<T, TRUE>
	{
		static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
			"T must be a fundamental type or a pointer type.");

	public:
		using Node = StackNode<T>;

		void Push(T data) noexcept
		{
			// ���� Push �� ���� �ĺ��� �߱�
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			ULONG_PTR readTop;
			Node *newTop = s_StackNodePool.Alloc();
			newTop->data = data;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

			do
			{
				// readTop ���� ident�� �ռ��� �ּ�
				readTop = m_pTop;
				newTop->next = readTop;

				// Sleep(0);

			} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

			// Push ����
			InterlockedIncrement(&m_iUseCount);
		}

		bool Pop(T *data) noexcept
		{
			LONG size = InterlockedDecrement(&m_iUseCount);
			if (size < 0)
			{
				InterlockedIncrement(&m_iUseCount);
				return false;
			}


			ULONG_PTR readTop;
			ULONG_PTR newTop;
			Node *readTopAddr;

			do
			{
				readTop = m_pTop;
				readTopAddr = (Node *)GetAddress(readTop);
				newTop = readTopAddr->next;

				// Sleep(0);

			} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

			*data = readTopAddr->data;
			s_StackNodePool.Free(readTopAddr);
			return true;
		}

	public:
		ULONG_PTR		m_pTop = NULL;
		ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
		LONG			m_iUseCount = 0;
		inline static CTLSMemoryPoolManager<Node> s_StackNodePool = CTLSMemoryPoolManager<Node>();
	};

	template<FundamentalOrPointer T>
	class CLFStack<T, FALSE>
	{
		static_assert(std::is_fundamental<T>::value || std::is_pointer<T>::value,
			"T must be a fundamental type or a pointer type.");

	public:
		using Node = StackNode<T>;

		void Push(T data) noexcept
		{
			// ���� Push �� ���� �ĺ��� �߱�
			ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
			ULONG_PTR readTop;
			Node *newTop = new Node;
			newTop->data = data;
			ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

			do
			{
				// readTop ���� ident�� �ռ��� �ּ�
				readTop = m_pTop;
				newTop->next = readTop;
			} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

			// Push ����
			InterlockedIncrement(&m_iUseCount);
		}

		void Pop(T *data)
		{
			LONG size = InterlockedDecrement(&m_iUseCount);
			if (size < 0)
			{
				InterlockedIncrement(&m_iUseCount);
				return false;
			}

			ULONG_PTR readTop;
			ULONG_PTR newTop;
			Node *readTopAddr;

			do
			{
				readTop = m_pTop;
				readTopAddr = (Node *)GetAddress(readTop);

				// ���⼭ �߻��� �� �ִ� ���ܴ� ������ decommit �� ������ ���� ����
				__try
				{
					newTop = readTopAddr->next;
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					continue;
				}

			} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

			// Pop ����
			InterlockedDecrement(&m_iUseCount);

			*data = readTopAddr->data;
			delete readTopAddr;
			return true;
		}

	public:
		ULONG_PTR		m_pTop = NULL;
		ULONG_PTR		m_ullCurrentIdentifier = 0; // ABA ������ �ذ��ϱ� ���� �ĺ���
		LONG			m_iUseCount = 0;
	};
}