#pragma once

#include "RegistriesTag.hpp"

#include <optional>
#include <string>
#include <vector>

class TagUtils {
  public:
	static std::optional<std::vector<Tag>> getTagsForRegistry(const std::string& registryName);

	static std::optional<Tag> getTag(const std::string& registryName, const std::string& tagName);

	static bool hasTag(const std::string& registryName, const std::string& tagName);

	static bool isInTag(const std::string& registryName, const std::string& tagName, int entryId);

	static std::vector<std::string> getAllRegistryNames();

	static size_t getTotalTagCount();

	static size_t getTagCountForRegistry(const std::string& registryName);

	static std::vector<int> getEntriesForTag(const std::string& registryName, const std::string& tagName);

	static void logTagStatistics();
};
