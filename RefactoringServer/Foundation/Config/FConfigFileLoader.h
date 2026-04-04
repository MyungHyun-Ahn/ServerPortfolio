#pragma once

#include "Foundation/Config/ConfigTypes.h"

namespace Foundation::Config
{
	class FConfigFileLoader
	{
	public:
		static bool LoadYamlFile(const std::filesystem::path& filePath, SConfigDocument& outDocument, std::string& outError);
	};
}
