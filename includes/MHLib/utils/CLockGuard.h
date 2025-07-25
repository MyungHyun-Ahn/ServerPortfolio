#pragma once

#include <windows.h>

namespace MHLib::utils
{
    enum class LOCK_TYPE
    {
        EXCLUSIVE,
        SHARED
    };

    template<LOCK_TYPE lockType>
    class CLockGuard
    {
    public:
        // 생성자: SRWLOCK 객체를 받아서 잠금을 획득
        explicit CLockGuard(SRWLOCK &lock) noexcept
            : m_lock(lock)
        {
            if constexpr (lockType == LOCK_TYPE::EXCLUSIVE)
            {
                AcquireSRWLockExclusive(&m_lock); // 쓰기 잠금
            }
            else
            {
                AcquireSRWLockShared(&m_lock); // 읽기 잠금
            }

            // // 연속 횟수 카운트 체크
            // DWORD id = GetCurrentThreadId();
            // if (threadId != id)
            // {
            //     threadId = id;
            //     switchCount++;
            // }
        }

        // 소멸자: SRWLOCK 잠금을 해제
        ~CLockGuard() noexcept
        {
            if constexpr (lockType == LOCK_TYPE::EXCLUSIVE)
            {
                ReleaseSRWLockExclusive(&m_lock); // 쓰기 잠금 해제
            }
            else
            {
                ReleaseSRWLockShared(&m_lock); // 읽기 잠금 해제
            }
        }

        // 복사 생성자와 복사 대입 연산자를 삭제하여 복사 방지
        CLockGuard(const CLockGuard &) = delete;
        CLockGuard &operator=(const CLockGuard &) = delete;

    private:
        SRWLOCK &m_lock; // 참조로 SRWLOCK 객체를 저장

    public:
        inline static DWORD threadId = 0;
        inline static LONG switchCount = 0;
    };
}