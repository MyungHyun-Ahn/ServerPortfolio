#pragma once

namespace NetworkLib::Core::Utils
{
	constexpr UINT64 SESSION_INDEX_MASK = 0xffff000000000000;
	constexpr UINT64 SESSION_ID_MASK = 0x0000ffffffffffff;
	constexpr INT WSASEND_MAX_BUFFER_COUNT = 512;

	inline USHORT GetSessionIndex(UINT64 sessionId) noexcept
	{
		UINT64 mask64 = sessionId & SESSION_INDEX_MASK;
		mask64 = mask64 >> 48;
		return (USHORT)mask64;
	}
	inline UINT64 GetSessionId(UINT64 sessionId) noexcept
	{
		UINT64 mask64 = sessionId & SESSION_ID_MASK;
		return mask64;
	}
	inline UINT64 CombineSessionIndex(USHORT index, UINT64 id) noexcept
	{
		UINT64 index64 = index;
		index64 = index64 << 48;
		return index64 | id;
	}

	namespace Net
	{
#pragma pack(push, 1)
		struct Header
		{
			BYTE code = 119;
			USHORT len;
			BYTE randKey;
			BYTE checkSum;
		};
#pragma pack(pop)
	}

	namespace Lan
	{
#pragma pack(push, 1)
		struct Header
		{
			USHORT len;
		};
#pragma pack(pop)
	}
}