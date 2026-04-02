#pragma once

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "ContentsRuntime/Core/ContentTypes.h"

#include <memory>

namespace NetworkLib
{
	class IServer;
}

namespace ContentsRuntime::Core
{
	class IContent;

	class FContentRuntime final : public Bridge::IContentBridge
	{
	public:
		FContentRuntime();
		~FContentRuntime() override;

		bool RegisterContent(std::unique_ptr<IContent> content);
		void Start(NetworkLib::IServer& server);
		void Stop();

		bool EnterSession(std::uint64_t sessionId, FContentId initialContentId);
		void LeaveSession(std::uint64_t sessionId);
		bool EnqueuePacket(std::uint64_t sessionId, std::uint16_t opcode, const char* payload, std::int32_t payloadLength);

	public:
		bool SendRaw(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) override;
		bool MoveSession(std::uint64_t sessionId, FContentId targetContentId) override;

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_impl;
	};
}
