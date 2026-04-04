#pragma once

#include "Foundation/Config/ConfigTypes.h"

#include <span>
#include <string_view>

namespace Foundation::Config
{
	class FConfigValueReader
	{
	public:
		explicit FConfigValueReader(const SConfigDocument& document) noexcept;

		bool ValidateKnownSections(std::span<const std::string_view> knownSectionNames, std::string& outError) const;
		bool ValidateKnownKeys(std::string_view sectionName, std::span<const std::string_view> knownKeyNames, std::string& outError) const;

		bool ReadRequiredString(std::string_view sectionName, std::string_view keyName, std::string& outValue, std::string& outError) const;
		bool ReadOptionalString(std::string_view sectionName, std::string_view keyName, std::string& outValue, std::string& outError) const;

		bool ReadRequiredBool(std::string_view sectionName, std::string_view keyName, bool& outValue, std::string& outError) const;
		bool ReadOptionalBool(std::string_view sectionName, std::string_view keyName, bool& outValue, std::string& outError) const;

		bool ReadRequiredInt32(std::string_view sectionName, std::string_view keyName, std::int32_t& outValue, std::string& outError) const;
		bool ReadOptionalInt32(std::string_view sectionName, std::string_view keyName, std::int32_t& outValue, std::string& outError) const;

		bool ReadRequiredUInt16(std::string_view sectionName, std::string_view keyName, std::uint16_t& outValue, std::string& outError) const;
		bool ReadOptionalUInt16(std::string_view sectionName, std::string_view keyName, std::uint16_t& outValue, std::string& outError) const;

		bool ReadRequiredUInt32(std::string_view sectionName, std::string_view keyName, std::uint32_t& outValue, std::string& outError) const;
		bool ReadOptionalUInt32(std::string_view sectionName, std::string_view keyName, std::uint32_t& outValue, std::string& outError) const;

		bool ReadRequiredInt64(std::string_view sectionName, std::string_view keyName, std::int64_t& outValue, std::string& outError) const;
		bool ReadOptionalInt64(std::string_view sectionName, std::string_view keyName, std::int64_t& outValue, std::string& outError) const;

		bool ReadRequiredUInt64(std::string_view sectionName, std::string_view keyName, std::uint64_t& outValue, std::string& outError) const;
		bool ReadOptionalUInt64(std::string_view sectionName, std::string_view keyName, std::uint64_t& outValue, std::string& outError) const;

		bool ReadRequiredFloat(std::string_view sectionName, std::string_view keyName, float& outValue, std::string& outError) const;
		bool ReadOptionalFloat(std::string_view sectionName, std::string_view keyName, float& outValue, std::string& outError) const;

		bool ReadRequiredDouble(std::string_view sectionName, std::string_view keyName, double& outValue, std::string& outError) const;
		bool ReadOptionalDouble(std::string_view sectionName, std::string_view keyName, double& outValue, std::string& outError) const;

	private:
		const SConfigSection* FindSection(std::string_view sectionName) const noexcept;
		const SConfigScalarValue* FindScalar(std::string_view sectionName, std::string_view keyName, std::string& outError) const;

		template <typename TValue>
		bool ReadValue(
			std::string_view sectionName,
			std::string_view keyName,
			bool required,
			TValue& outValue,
			std::string& outError) const;

	private:
		const SConfigDocument& m_document;
	};
}
