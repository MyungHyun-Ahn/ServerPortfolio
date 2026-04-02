#pragma once

#include "ContentsRuntime/Bridge/IContentBridge.h"
#include "ContentsRuntime/Core/ContentRuntimeTypes.h"

#include <memory>

namespace NetworkLib
{
	class IServer;
}

namespace ContentsRuntime::Core
{
	class IContent;
}

namespace ContentsRuntime::Routing
{
	class FContentRuntime final : public Bridge::IContentBridge
	{
	public:
		FContentRuntime();
		~FContentRuntime() override;

		bool RegisterContent(std::unique_ptr<Core::IContent> content);
		void SetConfig(const Core::SContentRuntimeConfig& config);
		void Start(NetworkLib::IServer& server);
		void Stop();
		Core::SContentRuntimeStats GetStatsSnapshot();

		bool EnterSession(std::uint64_t sessionId, Core::FContentId initialContentId);
		void LeaveSession(std::uint64_t sessionId);
		bool EnqueuePacket(std::uint64_t sessionId, std::uint16_t opcode, const char* payload, std::int32_t payloadLength);

	public:
		bool SendRaw(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) override;
		bool MoveSession(std::uint64_t sessionId, Core::FContentId targetContentId) override;
		bool DisconnectSession(std::uint64_t sessionId) override;
		bool IsSessionAlive(std::uint64_t sessionId) const override;
		std::optional<Core::FContentId> GetCurrentContentId(std::uint64_t sessionId) const override;

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_impl;
	};
}
