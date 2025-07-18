#pragma once

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

	// ���� �ִ� ũ��
	inline int GetCapacity() const noexcept { return m_iCapacity - 1; }

	// ���� ������� �뷮 ���
	inline int GetUseSize() const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return (r - f + m_iCapacity) % m_iCapacity;
	}

	// offset ���� ������� ������ ���
	inline int GetUseSize(int offset) const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return (r - (f + offset) + m_iCapacity) % m_iCapacity;
	}

	// ���� ���ۿ� ���� �뷮 ���
	inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize() - 1; }

	// ��� ������ ����
	inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; }

	// ������ ����
	int				Enqueue(char *data, int size) noexcept
	{
		// ���� ����
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

	// ������ ���� - buffer�� ����
	int				Dequeue(char *buffer, int size) noexcept
	{
		int ret = Peek(buffer, size);
		if (ret == -1)
			return -1;

		MoveFront(ret);
		return ret;
	}

	// ������ �����ϰ� �������� ����
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


	// ���� �����ͷ� �ܺο��� �����ϴ� �Լ���
	// �ܺο��� �ѹ��� ������ �� �ִ� ���� ��ȯ
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

	// Front, Rear Ŀ�� �̵�
	inline int				MoveRear(int size) noexcept { m_iRear = (m_iRear + size) % m_iCapacity; return m_iRear; }
	inline int				MoveFront(int size) noexcept { m_iFront = (m_iFront + size) % m_iCapacity; return m_iFront; }

	inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

	// Front ������ ���
	inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }

	// Rear ������ ���
	inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }

private:
	int				m_iCapacity = 4096 * 2; // ����Ʈ 1�� ����Ʈ ũ��, + 1

	int				m_iFront = 0;
	int				m_iRear = 0;

	char *m_PQueue = nullptr;
};