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
			// 최소 최대 워킹셋 수정
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

		// (내 OVERLAPPED 시작주소 - VirtualAlloc 할당 시작 주소) / sizeof(OVERLAPPED) * 3 = 내 OverlappedInfo 배열 시작 인덱스
		inline int GetAcceptExIndex(ULONG_PTR myOverlappedPtr) const noexcept
		{
			int AcceptExIndex = m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)];
			return AcceptExIndex;
		}

		inline void SetAcceptExIndex(ULONG_PTR myOverlappedPtr, int index) noexcept
		{
			m_arrAcceptExIndex[(myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) / (sizeof(OVERLAPPED) * 3)] = index;
		}

		// (내 OVERLAPPED 시작주소 - VirtualAlloc 할당 시작 주소) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED)
		// - 0 : AcceptEx
		// - 1 : Recv
		// - 2 : Send
		inline IOOperation CalOperation(ULONG_PTR myOverlappedPtr) const noexcept
		{
			IOOperation oper = (IOOperation)((myOverlappedPtr - (ULONG_PTR)m_pOverlappeds) % (sizeof(OVERLAPPED) * 3) / sizeof(OVERLAPPED));
			return oper;
		}

	private:
		// 64KB를 1개의 버킷으로 간주
		// 처음 할당할 때 연결된 메모리 영역을 할당 받을 것임
		// MAX_BUCKET_SIZE * 64KB의 영역을 할당
		// 1개의 버킷은 최대 682개의 세션을 받을 수 있음
		// ex) 30개의 버킷을 할당하면 20480개의 세션을 받을 수 있음

		static constexpr int MAX_BUCKET_SIZE = 30;
		LONG m_lSessionIndex = -1;
		char *m_pOverlappeds = nullptr;
		LONG m_arrAcceptExIndex[(64_kb * MAX_BUCKET_SIZE) / (sizeof(OVERLAPPED) * 3)];
	};
}