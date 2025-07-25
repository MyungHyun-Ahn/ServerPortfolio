#pragma once

namespace NetworkLib::DataStructures
{
	// ��� �����۶� ���� �����ε� �� ����� ����
	// - ��¥ RecvBuffer�� �� �뵵
	constexpr DWORD RECV_BUFFER_SIZE = 4096;

	class CRecvBuffer
	{
	public:
		CRecvBuffer() noexcept
		{
			m_PQueue = (char *)s_PagePool.Alloc();
		}
		~CRecvBuffer() noexcept
		{
			s_PagePool.Free(m_PQueue);
		}

		inline int GetCapacity() const noexcept { return m_iCapacity; }
		// ���� ������� �뷮 ���
		inline int GetUseSize() const noexcept
		{
			int r = m_iRear;
			int f = m_iFront;
			return r - f;
		}

		inline int GetUseSize(const int offset) const noexcept
		{
			int r = m_iRear;
			int f = m_iFront;
			return r - (f + offset);
		}

		// ���� ���ۿ� ���� �뷮 ���
		inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize(); }

		// ��� ������ ����
		inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; m_iRefCount = 0; }

		// ������ ���� ������ ���� ����� �ʿ� ����
		// * ����ȭ ���۷� ���� ����� ��
		// * Peek�� �ʿ�
		int Peek(char *buffer, int size) noexcept
		{
			if (GetUseSize() < size) {
				return -1;
			}

			memcpy_s(buffer, size, m_PQueue + m_iFront, size);
			return size;
		}
		int Peek(char *buffer, int size, int offset) noexcept
		{
			if (GetUseSize(offset) < size) {
				return -1;
			}

			int front = m_iFront + offset;

			memcpy_s(buffer, size, m_PQueue + front, size);

			return 0;
		}

		// Front, Rear Ŀ�� �̵�
		inline int				MoveRear(const int size) noexcept { m_iRear = (m_iRear + size); return m_iRear; }
		inline int				MoveFront(const int size) noexcept { m_iFront = (m_iFront + size); return m_iFront; }

		inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

		inline char *GetOffsetPtr(const int offset) const noexcept { return m_PQueue + offset; }

		// Front ������ ���
		inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }
		inline int GetFrontOffset() const noexcept { return m_iFront; }

		// Rear ������ ���
		inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }
		inline int GetRearOffset() const noexcept { return m_iRear; }

		inline LONG IncreaseRef() noexcept { return InterlockedIncrement(&m_iRefCount); }
		inline LONG DecreaseRef() noexcept { return InterlockedDecrement(&m_iRefCount); }

	private:
		char *m_PQueue = nullptr;

		int				m_iCapacity = RECV_BUFFER_SIZE;
		int				m_iFront = 0;
		int				m_iRear = 0;
		LONG			m_iRefCount = 0;

		USE_TLS_POOL_WITH_INIT(CRecvBuffer, s_sbufferPool, Clear)
		inline static MHLib::memory::CTLSPagePoolManager<RECV_BUFFER_SIZE, 2> s_PagePool = MHLib::memory::CTLSPagePoolManager<RECV_BUFFER_SIZE, 2>();
	};
}