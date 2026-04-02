#pragma once

namespace NetworkLib
{
	namespace Core
	{
		enum class EBackendKind : std::uint32_t;
		struct SServerConfig;
		struct SServerStats;
	}

	class IApplicationHandler;

	class IServer
	{
	public:
		virtual ~IServer() = default;

		virtual bool Start(const Core::SServerConfig& serverConfig, IApplicationHandler& applicationHandler) = 0;
		virtual void Stop() = 0;
		virtual bool Send(std::uint64_t sessionId, std::uint16_t opcode, const char* buffer, std::int32_t length) = 0;
		virtual Core::EBackendKind GetBackendKind() const = 0;
		virtual Core::SServerStats GetStatsSnapshot() const = 0;
	};
}
