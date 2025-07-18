#pragma once

namespace NetworkLib::DataStructures
{
	enum class IOOperation
	{
		ACCEPTEX,
		RECV,
		SEND,
		// System Event - 3 ~
		SENDPOST,
		RELEASE_SESSION,
		NONE = 400
	};

	inline constexpr unsigned long long operator ""_kb(unsigned long long val) { return val * 1024; }
	inline constexpr unsigned long long operator ""_mb(unsigned long long val) { return val * 1024 * 1024; }

	template<bool isPageLock = false>
	class COverlappedAllocator
	{
	public:
		COverlappedAllocator() noexcept
		{
			int err;
			// �ּ� �ִ� ��ŷ�� ����
			if constexpr (isPageLock)
			{
				SetProcessWorkingSetSize(GetCurrentProcess(), 1000_mb, (size_t)10000_mb);
			}

			m_pOverlappeds = (char *)VirtualAlloc(nullptr, 64_kb * MAX_BUCKET_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

			if constexpr (isPageLock)
			{
				if (VirtualLock(m_pOverlappeds, 64_kb * MAX_BUCKET_SIZE) == FALSE)
				{
					err = GetLastError();
					__debugbreak();
				}
			}
		}

		OVERLAPPED *Alloc() noexcept
		{
			LONG index = InterlockedIncrement(&m_lSessionIndex);
			OVERLAPPED *retOverlapped = (OVERLAPPED *)(m_pOverlappeds + (sizeof(OVERLAPPED) * 3 * index));
			return retOverlapped;
		}

		// (�� OVERLAPPED �����ּ� - VirtualAlloc �Ҵ� ���� �ּ�) / sizeof(OVERLAPPED) * 3 = �� OverlappedInfo �迭 ���� �ε���
		inline int GetAcceptExIndex(ULONG_PTR myOverlappedPtr) const noexcept
		{
			int AcceptExIndex = m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)];
			return AcceptExIndex;
		}

		inline void SetAcceptExIndex(ULONG_PTR myOverlappedPtr, int index) noexcept
		{
			m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)] = index;
		}

		// (�� OVERLAPPED �����ּ� - VirtualAlloc �Ҵ� ���� �ּ�) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED)
		// - 0 : AcceptEx
		// - 1 : Recv
		// - 2 : Send
		inline IOOperation CalOperation(ULONG_PTR myOverlappedPtr) const noexcept
		{
			IOOperation oper = (IOOperation)((myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED));
			return oper;
		}

	private:
		// 64KB�� 1���� ��Ŷ���� ����
		// ó�� �Ҵ��� �� ����� �޸� ������ �Ҵ� ���� ����
		// MAX_BUCKET_SIZE * 64KB�� ������ �Ҵ�
		// 1���� ��Ŷ�� �ִ� 682���� ������ ���� �� ����
		// ex) 30���� ��Ŷ�� �Ҵ��ϸ� 20480���� ������ ���� �� ����

		static constexpr int MAX_BUCKET_SIZE = 30;
		LONG m_lSessionIndex = -1;
		char *m_pOverlappeds = nullptr;
		LONG m_arrAcceptExIndex[(64_kb * MAX_BUCKET_SIZE) / (sizeof(OVERLAPPED) * 3)];
	};
}