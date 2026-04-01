#include "Pch.h"

#include "FFileLogger.h"

#include "LogFormatting.h"

namespace
{
	std::mutex g_fileMutex;
}

namespace GameServer::Foundation
{
	FFileLogger::FFileLogger(const SLogConfig& logConfig)
		: m_logConfig(logConfig)
	{
	}

	void FFileLogger::Log(ELogLevel logLevel, std::string_view category, std::string_view message)
	{
		if (!m_logConfig.fileEnabled || !Logging::ShouldWrite(m_logConfig, logLevel))
		{
			return;
		}

		std::filesystem::path directoryPath = std::filesystem::path(m_logConfig.outputDirectory) / std::string(category);
		std::filesystem::create_directories(directoryPath);

		std::filesystem::path filePath = directoryPath / (Logging::BuildDateStamp() + "_" + std::string(category) + ".log");
		const std::lock_guard<std::mutex> lock(g_fileMutex);
		std::ofstream outputFile(filePath, std::ios::out | std::ios::app);
		if (!outputFile.is_open())
		{
			return;
		}

		outputFile << Logging::BuildLine(m_logConfig, logLevel, category, message) << '\n';
	}
}
