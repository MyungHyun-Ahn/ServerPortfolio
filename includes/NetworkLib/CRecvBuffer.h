#pragma once

namespace NetworkLib::DataStructures
{
	// 사실 링버퍼랑 같은 역할인데 링 기능은 빠짐
	// - 진짜 RecvBuffer로 쓸 용도
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
		// 현재 사용중인 용량 얻기
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

		// 현재 버퍼에 남은 용량 얻기
		inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize(); }

		// 모든 데이터 삭제
		inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; m_iRefCount = 0; }

		// 데이터 삽입 데이터 추출 기능은 필요 없음
		// * 직렬화 버퍼로 만들어서 사용할 것
		// * Peek만 필요
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

		// Front, Rear 커서 이동
		inline int				MoveRear(const int size) noexcept { m_iRear = (m_iRear + size); return m_iRear; }
		inline int				MoveFront(const int size) noexcept { m_iFront = (m_iFront + size); return m_iFront; }

		inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

		inline char *GetOffsetPtr(const int offset) const noexcept { return m_PQueue + offset; }

		// Front 포인터 얻기
		inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }
		inline int GetFrontOffset() const noexcept { return m_iFront; }

		// Rear 포인터 얻기
		inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }
		inline int GetRearOffset() const noexcept { return m_iRear; }

	private:
		char *m_PQueue = nullptr;

		int				m_iCapacity = RECV_BUFFER_SIZE;
		int				m_iFront = 0;
		int				m_iRear = 0;
		
		USE_SMART_PTR
		USE_TLS_POOL_WITH_INIT(CRecvBuffer, s_sbufferPool, Clear)
		inline static MHLib::memory::CTLSPagePoolManager<RECV_BUFFER_SIZE, 2> s_PagePool = MHLib::memory::CTLSPagePoolManager<RECV_BUFFER_SIZE, 2>();
	};
}