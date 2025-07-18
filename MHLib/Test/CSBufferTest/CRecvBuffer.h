#pragma once
// ��� �����۶� ���� �����ε� �� ����� ����
// - ��¥ RecvBuffer�� �� �뵵
class CRecvBuffer
{
public:
	CRecvBuffer() noexcept
	{
		m_PQueue = new char[102400 * 2]; // (char *)s_PagePool8KB.Alloc();
		m_iCapacity = 102400 * 2;
	}
	~CRecvBuffer() noexcept
	{
		// s_PagePool8KB.Free(m_PQueue);
	}

	inline int GetCapacity() const noexcept { return m_iCapacity; }
	// ���� ������� �뷮 ���
	inline int GetUseSize() const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return r - f;
	}

	inline int GetUseSize(int offset) const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return r - (f + offset);
	}

	// ���� ���ۿ� ���� �뷮 ���
	inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize(); }

	// ��� ������ ����
	inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; m_iRefCount = 0; }

	int Enqueue(char *data, int size) noexcept
	{
		int freeSize = GetFreeSize();
		if (freeSize < size)
			return -1;

		int rear = m_iRear;
		memcpy_s(m_PQueue + rear, freeSize, data, size);
		MoveRear(size);
		return size;
	}

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
	inline int				MoveRear(int size) noexcept { m_iRear = (m_iRear + size); return m_iRear; }
	inline int				MoveFront(int size) noexcept { m_iFront = (m_iFront + size); return m_iFront; }

	inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

	inline char *GetOffsetPtr(int offset) const noexcept { return m_PQueue + offset; }

	// Front ������ ���
	inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }
	inline int GetFrontOffset() const noexcept { return m_iFront; }

	// Rear ������ ���
	inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }
	inline int GetRearOffset() const noexcept { return m_iRear; }

	inline LONG IncreaseRef() noexcept { return InterlockedIncrement(&m_iRefCount); }
	inline LONG DecreaseRef() noexcept { return InterlockedDecrement(&m_iRefCount); }

	inline static CRecvBuffer *Alloc() noexcept
	{
		CRecvBuffer *pBuffer = s_sbufferPool.Alloc();
		pBuffer->Clear();
		return pBuffer;
	}

	inline static void Free(CRecvBuffer *delBuffer) noexcept
	{
		s_sbufferPool.Free(delBuffer);
	}

	inline static LONG GetPoolCapacity() noexcept { return s_sbufferPool.GetCapacity(); }
	inline static LONG GetPoolUsage() noexcept { return s_sbufferPool.GetUseCount(); }

public:
	char *m_PQueue = nullptr;

	int				m_iCapacity = 5000;

	int				m_iFront = 0;
	int				m_iRear = 0;

	LONG			m_iRefCount = 0;

	inline static MHLib::memory::CTLSMemoryPoolManager<CRecvBuffer, 16, 4> s_sbufferPool = MHLib::memory::CTLSMemoryPoolManager<CRecvBuffer, 16, 4>();
	inline static MHLib::memory::CTLSPagePoolManager<4096, 2> s_PagePool8KB = MHLib::memory::CTLSPagePoolManager<4096, 2>();
};