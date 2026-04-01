#pragma once

#include <WinSock2.h>

#include <utility>
#include <vector>

namespace GameServer::NetworkLib
{
	class FSendBuffer
	{
	public:
		FSendBuffer() = default;
		explicit FSendBuffer(std::vector<char>&& buffer)
			: m_buffer(std::move(buffer))
		{
		}

		const char* GetData() const noexcept
		{
			return m_buffer.data();
		}

		char* GetData() noexcept
		{
			return m_buffer.data();
		}

		std::size_t GetSize() const noexcept
		{
			return m_buffer.size();
		}

		WSABUF MakeWsabuf() noexcept
		{
			WSABUF wsabuf{};
			wsabuf.buf = m_buffer.empty() ? nullptr : m_buffer.data();
			wsabuf.len = static_cast<ULONG>(m_buffer.size());
			return wsabuf;
		}

	private:
		std::vector<char> m_buffer;
	};
}
