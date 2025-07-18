#pragma once

namespace NetworkLib::DataStructures
{

	// CRecvBuffer�� ���� �����͸� ����ȭ ����ó�� �� �� �ְ� ��
	template<bool isLanServer>
	class CSerializableBufferView
	{
	public:
		CSerializableBufferView() noexcept = default;
		virtual ~CSerializableBufferView() noexcept = default;


	private:
		// ������ ���۰� ���� ����
		inline void Init(MHLib::utils::CSmartPtr<CRecvBuffer> spRecvBuffer, int offsetStart, int offsetEnd) noexcept
		{
			m_spRecvBuffer = spRecvBuffer;
			m_pBuffer = m_spRecvBuffer->GetOffsetPtr(offsetStart);
			m_HeaderFront = 0;
			m_Front = (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE;
			// Rear�� end - start
			m_Rear = offsetEnd - offsetStart;
			m_iReadHeaderSize = 0;
		}

		// �̰� �ϰ��� Copy�� �� �������
		inline void InitAndAlloc(int size) noexcept
		{
			m_pBuffer = new char[size];
			m_iBufferSize = size;
			m_HeaderFront = 0;
			m_Front = (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE;
			m_Rear = 0;
			m_iReadHeaderSize = 0;
		}

		inline void Clear() noexcept
		{
			m_iReadHeaderSize = 0;
			if (m_spRecvBuffer.GetRealPtr() != nullptr)
			{
				// Ref Count ���̰� ���� �� �ִٸ� ����
				m_spRecvBuffer.~CSmartPtr();
			}
			else
			{
				// ���� �Ҵ� ���� ������
				delete[] m_pBuffer;
			}
			m_pBuffer = nullptr;
			m_uiSessionId = 0;
		}
		// �̰� ���� ����� ������ �� ���̶�� ����ó�� �۾�
		inline USHORT isReadHeaderSize() noexcept
		{
			return m_iReadHeaderSize;
		}



		// �����º��� ���� �뵵
		bool Copy(char *buffer, int offset, int size) noexcept;
		// �ڷ� �̾ ���� �뵵
		bool Copy(char *buffer, int size) noexcept;

		bool GetHeader(char *buffer, int size) noexcept;

		inline int GetHeaderSize() const noexcept { return  (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE; }
		inline int GetFullSize() const noexcept { return GetDataSize() + GetHeaderSize(); }

		// �ܺο��� ���۸� ���� �����ϱ� ���� �뵵
		inline char *GetBufferPtr() const noexcept { return m_pBuffer; }
		inline char *GetContentBufferPtr() const noexcept { return m_pBuffer + m_Front; }
		inline int MoveWritePos(int size) noexcept { m_Rear += size; return m_Rear; }
		inline int MoveReadPos(int size) { m_Front += size; return m_Front; }

		// delayedHeader�� ����.
		bool WriteDelayedHeader(char *buffer, int size) noexcept;
		bool GetDelayedHeader(char *buffer, int size) noexcept;

		inline static CSerializableBufferView *Alloc() noexcept
		{
			CSerializableBufferView *pSBuffer = s_sbufferPool.Alloc();
			return pSBuffer;
		}

	public:
		inline int GetDataSize() const noexcept { return m_Rear - m_Front; }

		inline static void Free(CSerializableBufferView *delSBuffer) noexcept
		{
			// ���� �Ҵ� ���� ���۰� �ƴ϶�� recv ���۴� nullptr�� �ƴ�
			delSBuffer->Clear();
			s_sbufferPool.Free(delSBuffer);
		}

		// operator
	public:
		bool Dequeue(char *buffer, int size) noexcept;

		// ������ �ִ� �� �ʿ� ����
		inline CSerializableBufferView &operator>>(char &chData) noexcept
		{
			if (GetDataSize() < sizeof(char))
			{
				__debugbreak();
			}

			chData = *(char *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(char));

			return *this;
		}

		inline CSerializableBufferView &operator>>(unsigned char &byData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned char))
			{
				__debugbreak();
			}

			byData = *(unsigned char *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(unsigned char));

			return *this;
		}

		inline CSerializableBufferView &operator>>(short &shData) noexcept
		{
			if (GetDataSize() < sizeof(short))
			{
				__debugbreak();
			}

			shData = *(short *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(short));

			return *this;
		}

		inline CSerializableBufferView &operator>>(unsigned short &wData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned short))
			{
				__debugbreak();
			}

			wData = *(unsigned short *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(unsigned short));

			return *this;
		}

		inline CSerializableBufferView &operator>>(int &iData) noexcept
		{
			if (GetDataSize() < sizeof(int))
			{
				__debugbreak();
			}

			iData = *(int *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(int));

			return *this;
		}

		inline CSerializableBufferView &operator>>(long &lData) noexcept
		{
			if (GetDataSize() < sizeof(long))
			{
				__debugbreak();
			}

			lData = *(long *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(long));

			return *this;
		}

		inline CSerializableBufferView &operator>>(unsigned long &ulData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned long))
			{
				__debugbreak();
			}

			ulData = *(unsigned long *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(unsigned long));

			return *this;
		}

		inline CSerializableBufferView &operator>>(float &fData) noexcept
		{
			if (GetDataSize() < sizeof(float))
			{
				__debugbreak();
			}

			fData = *(float *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(float));

			return *this;
		}

		inline CSerializableBufferView &operator>>(__int64 &iData) noexcept
		{
			int size = GetDataSize();
			if (size < sizeof(__int64))
			{
				__debugbreak();
			}

			iData = *(__int64 *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(__int64));

			return *this;
		}

		inline CSerializableBufferView &operator>>(unsigned __int64 &uiData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned __int64))
			{
				__debugbreak();
			}

			uiData = *(unsigned __int64 *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(unsigned __int64));

			return *this;
		}

		inline CSerializableBufferView &operator>>(double &dData) noexcept
		{
			if (GetDataSize() < sizeof(double))
			{
				__debugbreak();
			}

			dData = *(double *)(m_pBuffer + m_Front);
			MoveReadPos(sizeof(double));

			return *this;
		}

		inline static LONG GetPoolCapacity() noexcept { return s_sbufferPool.GetCapacity(); }
		inline static LONG GetPoolUsage() noexcept { return s_sbufferPool.GetUseCount(); }

		inline LONG IncreaseRef() noexcept { return InterlockedIncrement(&m_iRefCount); }
		inline LONG DecreaseRef() noexcept { return InterlockedDecrement(&m_iRefCount); }


	private:
		char *m_pBuffer;
		int m_iBufferSize = 0;
		int m_HeaderFront = 0;
		int m_Front = 0;
		int m_Rear = 0;

		MHLib::utils::CSmartPtr<CRecvBuffer> m_spRecvBuffer;
		// ������ ��� ������ �и��� �����ϱ� ���� ����
		char m_delayedHeader[(int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE];
		USHORT m_iReadHeaderSize = 0;

		LONG			m_iRefCount = 0;
		UINT64			m_uiSessionId = 0;

		// inline static CLFMemoryPool<CSerializableBufferView> s_sbufferPool = CLFMemoryPool<CSerializableBufferView>(5000, false);
		inline static MHLib::memory::CTLSMemoryPoolManager <CSerializableBufferView, 8, 8, false> s_sbufferPool = MHLib::memory::CTLSMemoryPoolManager <CSerializableBufferView, 8, 8, false>();
	};

	// offset�� ���� 0�� �� ��?
	template<bool isLanServer>
	bool CSerializableBufferView<isLanServer>::Copy(char *buffer, int offset, int size) noexcept
	{
		if (m_iBufferSize - m_Rear < size)
		{
			// TODO: resize

			return false;
		}

		memcpy_s(m_pBuffer + offset, m_iBufferSize - m_Rear, buffer, size);

		// Copy �Լ��� ȣ��� ���� ������ HEADER�� ���� ������
		m_iReadHeaderSize = (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE;
		m_Rear = offset + size;

		return true;
	}

	template<bool isLanServer>
	bool CSerializableBufferView<isLanServer>::Copy(char *buffer, int size) noexcept
	{
		if (m_iBufferSize - m_Rear < size)
		{
			// TODO: resize

			return false;
		}

		memcpy_s(m_pBuffer + m_Rear, m_iBufferSize - m_Rear, buffer, size);

		// Copy �Լ��� ȣ��� ���� ������ HEADER�� ���� ������
		m_iReadHeaderSize = (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE;
		m_Rear += size;

		return true;
	}

	template<bool isLanServer>
	bool CSerializableBufferView<isLanServer>::Dequeue(char *buffer, int size) noexcept
	{
		if (GetDataSize() < size)
		{
			return false;
		}

		memcpy_s(buffer, size, m_pBuffer + m_Front, size);
		MoveReadPos(size);

		return true;
	}

	template<bool isLanServer>
	bool CSerializableBufferView<isLanServer>::GetHeader(char *buffer, int size) noexcept
	{
		if (size < (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE)
		{
			return false;
		}

		memcpy_s(buffer, size, m_pBuffer, size);

		return true;
	}

	template<bool isLanServer>
	bool CSerializableBufferView<isLanServer>::WriteDelayedHeader(char *buffer, int size) noexcept
	{
		if (m_iReadHeaderSize + size > (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE)
		{
			return false;
		}

		memcpy_s(m_delayedHeader + m_iReadHeaderSize, (int)CSerializableBuffer<isLanServer>::DEFINE::HEADER_SIZE, buffer, size);
		m_iReadHeaderSize += size;
		return true;
	}

	template<bool isLanServer>
	bool CSerializableBufferView<isLanServer>::GetDelayedHeader(char *buffer, int size) noexcept
	{
		if (size < m_iReadHeaderSize)
		{
			return false;
		}

		memcpy_s(buffer, size, m_delayedHeader, size);

		return true;
	}
}