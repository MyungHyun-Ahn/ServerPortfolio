#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Generated::Config::EchoServer
{
	struct SEchoServerConfig
	{
		std::string Backend = "Iocp";
		std::string BindIp = "127.0.0.1";
		std::uint16_t Port = static_cast<std::uint16_t>(19000);
		std::int32_t WorkerThreadCount = static_cast<std::int32_t>(2);
		std::int32_t MaxSessionCount = static_cast<std::int32_t>(512);
		std::int32_t RecvBufferSize = static_cast<std::int32_t>(1024);
		std::string LogMinimumLevel = "Info";
		std::string LogOutputDirectory = "";
		bool LogConsoleEnabled = true;
		bool LogFileEnabled = true;
		bool LogIncludeThreadId = true;
		std::uint32_t PacketKey = static_cast<std::uint32_t>(55);
		bool EnablePagePool = true;
		std::uint32_t PageSize = static_cast<std::uint32_t>(4096);
		std::int32_t SendThreadCount = static_cast<std::int32_t>(1);
		std::int32_t ResponsesPerThread = static_cast<std::int32_t>(1);
		std::int32_t RoomCount = static_cast<std::int32_t>(50);
		std::int32_t RoomCapacity = static_cast<std::int32_t>(4);
	};

	struct SEchoServerDebugConfig
	{
		bool ManualDump = false;
		bool Headless = false;
		bool BootstrapTrace = false;
		std::uint32_t TraceUserId = static_cast<std::uint32_t>(0);
		bool LogPackets = false;
		bool TransitionRaceInjectionEnabled = false;
		std::string TransitionRaceInjectionMode = "None";
		bool PostRoomChangeRaceInjectionEnabled = false;
		std::string PostRoomChangeRaceInjectionMode = "None";
		bool FirstEchoRaceInjectionEnabled = false;
		std::string FirstEchoRaceInjectionMode = "None";
		bool ContentsRaceInjectionEnabled = false;
		std::uint32_t ContentsRaceInjectionPeriod = static_cast<std::uint32_t>(100);
		std::string ContentsRaceInjectionMode = "None";
		bool ContentsFailFast = false;
	};

	struct FEchoServerConfigDocument
	{
		SEchoServerConfig EchoServer;
		SEchoServerDebugConfig Debug;
	};

	class FEchoServerConfigLoader
	{
	public:
		static bool LoadFromFile(const std::filesystem::path& filePath, FEchoServerConfigDocument& outConfig, std::string& outError);
	};
}
