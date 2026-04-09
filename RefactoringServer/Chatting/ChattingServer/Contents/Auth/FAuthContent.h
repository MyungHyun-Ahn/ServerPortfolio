#pragma once

#include "ContentsRuntime/Core/IContent.h"
#include "ChattingServer/Contents/ContentTypes.h"
#include "ChattingServer/Contents/Session/FUserRegistry.h"
#include "Connector/Interfaces/IChatTicketStore.h"
#include "Foundation/Logging/ILogger.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace ChattingServer::Contents
{
	class FAuthContent final : public ContentsRuntime::Core::IContent
	{
	public:
		FAuthContent(
			std::shared_ptr<Foundation::ILogger> logger,
			ContentsRuntime::Core::FContentInstanceId contentInstanceId,
			std::shared_ptr<FUserRegistry> userRegistry,
			std::shared_ptr<Connector::IChatTicketStore> chatTicketStore,
			SRuntimeOptions runtimeOptions);

		ContentsRuntime::Core::FContentId GetContentId() const noexcept override;
		ContentsRuntime::Core::FContentInstanceId GetContentInstanceId() const noexcept override;
		void OnEnter(std::uint64_t sessionId, std::uint64_t routeGeneration, ContentsRuntime::Bridge::IContentBridge& bridge) override;
		void OnLeave(std::uint64_t sessionId, std::uint64_t routeGeneration, ContentsRuntime::Bridge::IContentBridge& bridge) override;
		void OnPacket(
			std::uint64_t sessionId,
			std::uint64_t routeGeneration,
			std::uint16_t opcode,
			std::span<const char> payload,
			ContentsRuntime::Bridge::IContentBridge& bridge) override;

	private:
		void Log(Foundation::ELogLevel logLevel, const std::string& message) const;

	private:
		std::shared_ptr<Foundation::ILogger> m_logger;
		ContentsRuntime::Core::FContentInstanceId m_contentInstanceId = ContentsRuntime::Core::kInvalidContentInstanceId;
		std::shared_ptr<FUserRegistry> m_userRegistry;
		std::shared_ptr<Connector::IChatTicketStore> m_chatTicketStore;
		SRuntimeOptions m_runtimeOptions;
		std::unordered_map<std::uint64_t, std::uint64_t> m_sessionGenerations;
	};
}
