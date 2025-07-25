#pragma once

namespace NetworkLib::DataStructures
{
	template<int bufferSize = 4096>
	class CRingBuffer
	{
	public:
		CRingBuffer() noexcept
		{
			m_PQueue = new char[m_iCapacity];
		}
		CRingBuffer(int size) noexcept : m_iCapacity(size + 1)
		{
			m_PQueue = new char[m_iCapacity];
		}
		~CRingBuffer() noexcept
		{
			delete m_PQueue;
		}

		// 버퍼 최대 크기
		inline int GetCapacity() const noexcept { return m_iCapacity - 1; }

		// 현재 사용중인 용량 얻기
		inline int GetUseSize() const noexcept
		{
			int r = m_iRear;
			int f = m_iFront;
			return (r - f + m_iCapacity) % m_iCapacity;
		}

		// offset 부터 사용중인 사이즈 계산
		inline int GetUseSize(int offset) const noexcept
		{
			int r = m_iRear;
			int f = m_iFront;
			return (r - (f + offset) + m_iCapacity) % m_iCapacity;
		}

		// 현재 버퍼에 남은 용량 얻기
		inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize() - 1; }

		// 모든 데이터 삭제
		inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; }

		// 데이터 삽입
		int				Enqueue(char *data, int size) noexcept
		{
			// 공간 부족
			int freeSize = GetFreeSize();
			if (freeSize < size)
				return -1;

			int rear = m_iRear;
			int des = DirectEnqueueSize();
			int firstPartSize = 0;
			if (des < 0)
				firstPartSize = size;
			else
				firstPartSize = min(size, des);

			int secondPartSize = size - firstPartSize;
			memcpy_s(m_PQueue + rear, freeSize, data, firstPartSize);
			memcpy_s(m_PQueue, secondPartSize, data + firstPartSize, secondPartSize);
			MoveRear(size);

			return size;
		}

		// 데이터 추출 - buffer에 저장
		int				Dequeue(char *buffer, int size) noexcept
		{
			int ret = Peek(buffer, size);
			if (ret == -1)
				return -1;

			MoveFront(ret);
			return ret;
		}

		// 데이터 추출하고 꺼내지는 않음
		int				Peek(char *buffer, int size) noexcept
		{
			if (GetUseSize() < size) {
				return -1;
			}

			int front = m_iFront;
			int dds = DirectDequeueSize();
			int firstPartSize = 0;

			if (dds < 0)
				firstPartSize = size;
			else
				firstPartSize = min(size, dds);
			int secondPartSize = size - firstPartSize;

			memcpy_s(buffer, size, m_PQueue + front, firstPartSize);
			memcpy_s(buffer + firstPartSize, size - firstPartSize, m_PQueue, secondPartSize);

			return size;
		}
		int				Peek(char *buffer, int size, int offset) noexcept
		{
			if (GetUseSize(offset) < size) {
				return -1;
			}

			int front = m_iFront;
			front = (front + offset) % m_iCapacity;
			int dds = DirectDequeueSize(offset);
			int firstPartSize = 0;

			if (dds < 0)
				firstPartSize = size;
			else
				firstPartSize = min(size, dds);

			int secondPartSize = size - firstPartSize;

			memcpy_s(buffer, size, m_PQueue + front, firstPartSize);
			memcpy_s(buffer + firstPartSize, size - firstPartSize, m_PQueue, secondPartSize);

			return 0;
		}


		// 버퍼 포인터로 외부에서 조작하는 함수들
		// 외부에서 한번에 조작할 수 있는 길이 반환
		inline int DirectEnqueueSize() const noexcept
		{
			int f = m_iFront;
			int r = m_iRear;

			if (f <= r)
			{
				return m_iCapacity - r - (f == 0 ? 1 : 0);
			}
			else
			{
				return f - r - 1;
			}
		}
		inline int DirectDequeueSize() const noexcept
		{
			int f = m_iFront;
			int r = m_iRear;

			if (f <= r)
			{
				return r - f;
			}
			else
			{
				return m_iCapacity - f;
			}
		}

		inline int DirectDequeueSize(int offset) const noexcept
		{
			int f = m_iFront;
			f = (f + offset) % m_iCapacity;
			int r = m_iRear;

			if (f <= r)
			{
				return r - f;
			}
			else
			{
				return m_iCapacity - f;
			}
		}

		// Front, Rear 커서 이동
		inline int				MoveRear(int size) noexcept { m_iRear = (m_iRear + size) % m_iCapacity; return m_iRear; }
		inline int				MoveFront(int size) noexcept { m_iFront = (m_iFront + size) % m_iCapacity; return m_iFront; }

		inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

		// Front 포인터 얻기
		inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }

		// Rear 포인터 얻기
		inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }

	private:
		int				m_iCapacity = bufferSize;

		int				m_iFront = 0;
		int				m_iRear = 0;

		char *m_PQueue = nullptr;
	};
}