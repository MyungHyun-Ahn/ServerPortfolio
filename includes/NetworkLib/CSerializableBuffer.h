#pragma once

namespace NetworkLib::Core::Net::Server
{
	class CNetServer;
}

namespace NetworkLib::DataStructures
{
	template<SERVER_TYPE serverType, DWORD bufferSize = 512>
	class CSerializableBuffer
	{
	public:
		friend class NetworkLib::Core::Net::Server::CNetServer;

		enum class DEFINE : int
		{
			HEADER_SIZE = (serverType == SERVER_TYPE::LAN) ? HEADER_SIZE::LAN : HEADER_SIZE::NET,
			PACKET_MAX_SIZE = bufferSize
		};

		CSerializableBuffer() noexcept
		{
			m_Buffer = (char *)s_sPagePool.Alloc();
			m_MaxSize = static_cast<int>(DEFINE::PACKET_MAX_SIZE);
			m_HeaderFront = 0;
			m_Front = static_cast<int>(DEFINE::HEADER_SIZE);
			m_Rear = static_cast<int>(DEFINE::HEADER_SIZE);
		}

		virtual ~CSerializableBuffer() noexcept
		{
			s_sPagePool.Free(m_Buffer);
		}


		inline void Clear() noexcept
		{
			m_HeaderFront = 0;
			m_Front = static_cast<int>(DEFINE::HEADER_SIZE);
			m_Rear = static_cast<int>(DEFINE::HEADER_SIZE);
			m_isEnqueueHeader = FALSE;
		}

		bool EnqueueHeader(const char *buffer, const int size) noexcept
		{
			m_isEnqueueHeader = TRUE;

			if (m_HeaderFront + size > m_Front)
				return true;

			memcpy_s(m_Buffer + m_HeaderFront, m_Front - m_HeaderFront, buffer, size);

			m_HeaderFront += size;

			return true;
		}

		bool Enqueue(const char *buffer, const int size) noexcept
		{
			if (m_MaxSize - m_Rear < size)
				return false;

			memcpy_s(m_Buffer + m_Rear, m_MaxSize - m_Rear, buffer, size);
			MoveWritePos(size);

			return true;
		}

		bool Dequeue(char *buffer, const int size) noexcept
		{
			if (GetDataSize() < size)
				return false;

			memcpy_s(buffer, size, m_Buffer + m_Front, size);
			MoveReadPos(size);

			return true;
		}

		inline int GetBufferSize() const noexcept { return m_MaxSize; }
		inline int GetDataSize() const noexcept { return m_Rear - m_Front; }
		inline int GetHeaderSize() const noexcept { return static_cast<int>(DEFINE::HEADER_SIZE); }
		inline int GetFullSize() const noexcept { return GetDataSize() + GetHeaderSize(); }
		inline int GetIsEnqueueHeader() const noexcept { return m_isEnqueueHeader; }

		// 외부에서 버퍼를 직접 조작하기 위한 용도
		inline char *GetBufferPtr() const noexcept { return m_Buffer; }
		inline char *GetContentBufferPtr() const noexcept { return m_Buffer + m_Front; }
		inline int MoveWritePos(int size) noexcept { m_Rear += size; return m_Rear; }
		inline int MoveReadPos(int size) noexcept { m_Front += size; return m_Front; }

	public:
		inline CSerializableBuffer &operator<<(char chData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(char))
			{
				// TODO: resize
			}

			char *ptr = (char *)(m_Buffer + m_Rear);
			*ptr = chData;

			MoveWritePos(sizeof(char));

			return *this;
		}

		inline CSerializableBuffer &operator<<(unsigned char byData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(unsigned char))
			{
				// TODO: resize
			}

			unsigned char *ptr = (unsigned char *)(m_Buffer + m_Rear);
			*ptr = byData;

			MoveWritePos(sizeof(unsigned char));

			return *this;
		}

		inline CSerializableBuffer &operator<<(short shData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(short))
			{
				// TODO: resize
			}

			short *ptr = (short *)(m_Buffer + m_Rear);
			*ptr = shData;

			MoveWritePos(sizeof(short));

			return *this;
		}

		inline CSerializableBuffer &operator<<(unsigned short wData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(unsigned short))
			{
				// TODO: resize
			}

			unsigned short *ptr = (unsigned short *)(m_Buffer + m_Rear);
			*ptr = wData;

			MoveWritePos(sizeof(unsigned short));

			return *this;
		}

		inline CSerializableBuffer &operator<<(int iData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(int))
			{
				// TODO: resize
			}

			int *ptr = (int *)(m_Buffer + m_Rear);
			*ptr = iData;

			MoveWritePos(sizeof(int));

			return *this;
		}

		inline CSerializableBuffer &operator<<(long lData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(long))
			{
				// TODO: resize
			}

			long *ptr = (long *)(m_Buffer + m_Rear);
			*ptr = lData;

			MoveWritePos(sizeof(long));

			return *this;
		}

		inline CSerializableBuffer &operator<<(unsigned long lData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(unsigned long))
			{
				// TODO: resize
			}

			unsigned long *ptr = (unsigned long *)(m_Buffer + m_Rear);
			*ptr = lData;

			MoveWritePos(sizeof(unsigned long));

			return *this;
		}

		inline CSerializableBuffer &operator<<(float fData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(float))
			{
				// TODO: resize
			}

			float *ptr = (float *)(m_Buffer + m_Rear);
			*ptr = fData;

			MoveWritePos(sizeof(float));

			return *this;
		}

		inline CSerializableBuffer &operator<<(__int64 iData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(__int64))
			{
				// TODO: resize
			}

			__int64 *ptr = (__int64 *)(m_Buffer + m_Rear);
			*ptr = iData;

			MoveWritePos(sizeof(__int64));

			return *this;
		}

		inline CSerializableBuffer &operator<<(unsigned __int64 uiData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(unsigned __int64))
			{
				// TODO: resize
			}

			unsigned __int64 *ptr = (unsigned __int64 *)(m_Buffer + m_Rear);
			*ptr = uiData;

			MoveWritePos(sizeof(unsigned __int64));

			return *this;
		}

		inline CSerializableBuffer &operator<<(double dData) noexcept
		{
			if (m_MaxSize - m_Rear > sizeof(double))
			{
				// TODO: resize
			}

			*(double *)(m_Buffer + m_Rear) = dData;

			MoveWritePos(sizeof(double));

			return *this;
		}

		inline CSerializableBuffer &operator>>(char &chData) noexcept
		{
			if (GetDataSize() < sizeof(char))
			{
				__debugbreak();
			}

			chData = *(char *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(char));

			return *this;
		}

		inline CSerializableBuffer &operator>>(unsigned char &byData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned char))
			{
				__debugbreak();
			}

			byData = *(unsigned char *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(unsigned char));

			return *this;
		}

		inline CSerializableBuffer &operator>>(short &shData) noexcept
		{
			if (GetDataSize() < sizeof(short))
			{
				__debugbreak();
			}

			shData = *(short *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(short));

			return *this;
		}

		inline CSerializableBuffer &operator>>(unsigned short &wData) noexcept
		{
			if (GetDataSize() < sizeof(char))
			{
				__debugbreak();
			}

			wData = *(unsigned short *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(unsigned short));

			return *this;
		}

		inline CSerializableBuffer &operator>>(int &iData) noexcept
		{
			if (GetDataSize() < sizeof(int))
			{
				__debugbreak();
			}

			iData = *(int *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(int));

			return *this;
		}

		inline CSerializableBuffer &operator>>(long &lData) noexcept
		{
			if (GetDataSize() < sizeof(long))
			{
				__debugbreak();
			}

			lData = *(long *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(long));

			return *this;
		}

		inline CSerializableBuffer &operator>>(unsigned long &ulData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned long))
			{
				__debugbreak();
			}

			ulData = *(unsigned long *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(unsigned long));

			return *this;
		}

		inline CSerializableBuffer &operator>>(float &fData) noexcept
		{
			if (GetDataSize() < sizeof(float))
			{
				__debugbreak();
			}

			fData = *(float *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(float));

			return *this;
		}

		inline CSerializableBuffer &operator>>(__int64 &iData) noexcept
		{
			if (GetDataSize() < sizeof(__int64))
			{
				__debugbreak();
			}

			iData = *(__int64 *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(__int64));

			return *this;
		}

		inline CSerializableBuffer &operator>>(unsigned __int64 &uiData) noexcept
		{
			if (GetDataSize() < sizeof(unsigned __int64))
			{
				__debugbreak();
			}

			uiData = *(unsigned __int64 *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(unsigned __int64));

			return *this;
		}

		inline CSerializableBuffer &operator>>(double &dData) noexcept
		{
			if (GetDataSize() < sizeof(double))
			{
				__debugbreak();
			}

			dData = *(double *)(m_Buffer + m_Front);
			MoveReadPos(sizeof(double));

			return *this;
		}

	public:
		inline void SetSessionId(UINT64 id) noexcept { m_uiSessionId = id; }
		inline UINT64 GetSessionId() noexcept { return m_uiSessionId; }

	private:
		char *m_Buffer;
		int m_HeaderFront = 0;
		int m_Front = 0;
		int m_Rear = 0;
		int m_MaxSize = (int)DEFINE::PACKET_MAX_SIZE;

		BOOL			m_isEnqueueHeader = 0;
		UINT64			m_uiSessionId = 0;

		USE_SMART_PTR

		USE_TLS_POOL_WITH_INIT(CSerializableBuffer, s_sbufferPool, Clear)
		inline static MHLib::memory::CTLSPagePoolManager<(int)DEFINE::PACKET_MAX_SIZE, 16, false> s_sPagePool = MHLib::memory::CTLSPagePoolManager<(int)DEFINE::PACKET_MAX_SIZE, 16, false>();
	};
}