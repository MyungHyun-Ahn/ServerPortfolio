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
        // ������: SRWLOCK ��ü�� �޾Ƽ� ����� ȹ��
        explicit CLockGuard(SRWLOCK &lock) noexcept
            : m_lock(lock)
        {
            if constexpr (lockType == LOCK_TYPE::EXCLUSIVE)
            {
                AcquireSRWLockExclusive(&m_lock); // ���� ���
            }
            else
            {
                AcquireSRWLockShared(&m_lock); // �б� ���
            }

            // // ���� Ƚ�� ī��Ʈ üũ
            // DWORD id = GetCurrentThreadId();
            // if (threadId != id)
            // {
            //     threadId = id;
            //     switchCount++;
            // }
        }

        // �Ҹ���: SRWLOCK ����� ����
        ~CLockGuard() noexcept
        {
            if constexpr (lockType == LOCK_TYPE::EXCLUSIVE)
            {
                ReleaseSRWLockExclusive(&m_lock); // ���� ��� ����
            }
            else
            {
                ReleaseSRWLockShared(&m_lock); // �б� ��� ����
            }
        }

        // ���� �����ڿ� ���� ���� �����ڸ� �����Ͽ� ���� ����
        CLockGuard(const CLockGuard &) = delete;
        CLockGuard &operator=(const CLockGuard &) = delete;

    private:
        SRWLOCK &m_lock; // ������ SRWLOCK ��ü�� ����

    public:
        inline static DWORD threadId = 0;
        inline static LONG switchCount = 0;
    };
}