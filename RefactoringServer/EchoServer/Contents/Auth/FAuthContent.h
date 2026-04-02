#pragma once

#include "ContentsRuntime/Core/IContent.h"
#include "Foundation/Logging/ILogger.h"

namespace EchoServer::Contents
{
	class FAuthContent final : public ContentsRuntime::Core::IContent
	{
	public:
		explicit FAuthContent(std::shared_ptr<Foundation::ILogger> logger);

		ContentsRuntime::Core::FContentId GetContentId() const noexcept override;
		void OnEnter(std::uint64_t sessionId, ContentsRuntime::Bridge::IContentBridge& bridge) override;
		void OnLeave(std::uint64_t sessionId, ContentsRuntime::Bridge::IContentBridge& bridge) override;
		void OnPacket(
			std::uint64_t sessionId,
			std::uint16_t opcode,
			std::span<const char> payload,
			ContentsRuntime::Bridge::IContentBridge& bridge) override;

	private:
		void Log(Foundation::ELogLevel logLevel, const std::string& message) const;

	private:
		std::shared_ptr<Foundation::ILogger> m_logger;
	};
}
