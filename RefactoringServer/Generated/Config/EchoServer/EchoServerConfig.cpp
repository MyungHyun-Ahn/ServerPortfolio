#include "Pch.h"

#include "Generated/Config/EchoServer/EchoServerConfig.h"
#include "Foundation/Config/FConfigFileLoader.h"
#include "Foundation/Config/FConfigValueReader.h"

#include <array>
#include <string_view>

namespace Generated::Config::EchoServer
{
	bool FEchoServerConfigLoader::LoadFromFile(const std::filesystem::path& filePath, FEchoServerConfigDocument& outConfig, std::string& outError)
	{
		Foundation::Config::SConfigDocument document{};
		if (!Foundation::Config::FConfigFileLoader::LoadYamlFile(filePath, document, outError))
		{
			return false;
		}

		Foundation::Config::FConfigValueReader reader(document);

		constexpr std::array<std::string_view, 2> kKnownSections =
		{
			"EchoServer",
			"Debug"
		};

		if (!reader.ValidateKnownSections(kKnownSections, outError))
		{
			return false;
		}

		constexpr std::array<std::string_view, 18> kEchoServerKnownKeys =
		{
			"Backend",
			"BindIp",
			"Port",
			"WorkerThreadCount",
			"MaxSessionCount",
			"RecvBufferSize",
			"LogMinimumLevel",
			"LogOutputDirectory",
			"LogConsoleEnabled",
			"LogFileEnabled",
			"LogIncludeThreadId",
			"PacketKey",
			"EnablePagePool",
			"PageSize",
			"SendThreadCount",
			"ResponsesPerThread",
			"RoomCount",
			"RoomCapacity"
		};

		if (!reader.ValidateKnownKeys("EchoServer", kEchoServerKnownKeys, outError))
		{
			return false;
		}

		constexpr std::array<std::string_view, 15> kDebugKnownKeys =
		{
			"ManualDump",
			"Headless",
			"BootstrapTrace",
			"TraceUserId",
			"LogPackets",
			"TransitionRaceInjectionEnabled",
			"TransitionRaceInjectionMode",
			"PostRoomChangeRaceInjectionEnabled",
			"PostRoomChangeRaceInjectionMode",
			"FirstEchoRaceInjectionEnabled",
			"FirstEchoRaceInjectionMode",
			"ContentsRaceInjectionEnabled",
			"ContentsRaceInjectionPeriod",
			"ContentsRaceInjectionMode",
			"ContentsFailFast"
		};

		if (!reader.ValidateKnownKeys("Debug", kDebugKnownKeys, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("EchoServer", "Backend", outConfig.EchoServer.Backend, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("EchoServer", "BindIp", outConfig.EchoServer.BindIp, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalUInt16("EchoServer", "Port", outConfig.EchoServer.Port, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "WorkerThreadCount", outConfig.EchoServer.WorkerThreadCount, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "MaxSessionCount", outConfig.EchoServer.MaxSessionCount, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "RecvBufferSize", outConfig.EchoServer.RecvBufferSize, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("EchoServer", "LogMinimumLevel", outConfig.EchoServer.LogMinimumLevel, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("EchoServer", "LogOutputDirectory", outConfig.EchoServer.LogOutputDirectory, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("EchoServer", "LogConsoleEnabled", outConfig.EchoServer.LogConsoleEnabled, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("EchoServer", "LogFileEnabled", outConfig.EchoServer.LogFileEnabled, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("EchoServer", "LogIncludeThreadId", outConfig.EchoServer.LogIncludeThreadId, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalUInt32("EchoServer", "PacketKey", outConfig.EchoServer.PacketKey, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("EchoServer", "EnablePagePool", outConfig.EchoServer.EnablePagePool, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalUInt32("EchoServer", "PageSize", outConfig.EchoServer.PageSize, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "SendThreadCount", outConfig.EchoServer.SendThreadCount, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "ResponsesPerThread", outConfig.EchoServer.ResponsesPerThread, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "RoomCount", outConfig.EchoServer.RoomCount, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalInt32("EchoServer", "RoomCapacity", outConfig.EchoServer.RoomCapacity, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "ManualDump", outConfig.Debug.ManualDump, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "Headless", outConfig.Debug.Headless, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "BootstrapTrace", outConfig.Debug.BootstrapTrace, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalUInt32("Debug", "TraceUserId", outConfig.Debug.TraceUserId, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "LogPackets", outConfig.Debug.LogPackets, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "TransitionRaceInjectionEnabled", outConfig.Debug.TransitionRaceInjectionEnabled, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("Debug", "TransitionRaceInjectionMode", outConfig.Debug.TransitionRaceInjectionMode, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "PostRoomChangeRaceInjectionEnabled", outConfig.Debug.PostRoomChangeRaceInjectionEnabled, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("Debug", "PostRoomChangeRaceInjectionMode", outConfig.Debug.PostRoomChangeRaceInjectionMode, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "FirstEchoRaceInjectionEnabled", outConfig.Debug.FirstEchoRaceInjectionEnabled, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("Debug", "FirstEchoRaceInjectionMode", outConfig.Debug.FirstEchoRaceInjectionMode, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "ContentsRaceInjectionEnabled", outConfig.Debug.ContentsRaceInjectionEnabled, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalUInt32("Debug", "ContentsRaceInjectionPeriod", outConfig.Debug.ContentsRaceInjectionPeriod, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalString("Debug", "ContentsRaceInjectionMode", outConfig.Debug.ContentsRaceInjectionMode, outError))
		{
			return false;
		}

		if (!reader.ReadOptionalBool("Debug", "ContentsFailFast", outConfig.Debug.ContentsFailFast, outError))
		{
			return false;
		}

		return true;
	}
}
