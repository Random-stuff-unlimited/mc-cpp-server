#include "data/TagUtils.hpp"

#include "logger.hpp"

#include <algorithm>

std::optional<std::vector<Tag>> TagUtils::getTagsForRegistry(const std::string& registryName) {
	auto it = RegistriesTags.find(registryName);
	if (it != RegistriesTags.end()) {
		return it->second;
	}
	return std::nullopt;
}

std::optional<Tag> TagUtils::getTag(const std::string& registryName, const std::string& tagName) {
	auto registryIt = RegistriesTags.find(registryName);
	if (registryIt != RegistriesTags.end()) {
		auto tagIt = std::find_if(registryIt->second.begin(), registryIt->second.end(), [&tagName](const Tag& tag) { return tag.name == tagName; });
		if (tagIt != registryIt->second.end()) {
			return *tagIt;
		}
	}
	return std::nullopt;
}

bool TagUtils::hasTag(const std::string& registryName, const std::string& tagName) { return getTag(registryName, tagName).has_value(); }

bool TagUtils::isInTag(const std::string& registryName, const std::string& tagName, int entryId) {
	auto tag = getTag(registryName, tagName);
	if (tag.has_value()) {
		const auto& entries = tag->entries;
		return std::find(entries.begin(), entries.end(), entryId) != entries.end();
	}
	return false;
}

std::vector<std::string> TagUtils::getAllRegistryNames() {
	std::vector<std::string> names;
	names.reserve(RegistriesTags.size());
	for (const auto& [name, tags] : RegistriesTags) {
		names.push_back(name);
	}
	return names;
}

size_t TagUtils::getTotalTagCount() {
	size_t total = 0;
	for (const auto& [registryName, tags] : RegistriesTags) {
		total += tags.size();
	}
	return total;
}

size_t TagUtils::getTagCountForRegistry(const std::string& registryName) {
	auto it = RegistriesTags.find(registryName);
	if (it != RegistriesTags.end()) {
		return it->second.size();
	}
	return 0;
}

std::vector<int> TagUtils::getEntriesForTag(const std::string& registryName, const std::string& tagName) {
	auto tag = getTag(registryName, tagName);
	if (tag.has_value()) {
		return tag->entries;
	}
	return {};
}

void TagUtils::logTagStatistics() {
	if (!g_logger) return;

	size_t totalTags = getTotalTagCount();
	g_logger->logGameInfo(INFO, "=== Tag Statistics ===", "TagUtils");
	g_logger->logGameInfo(INFO, "Total registries: " + std::to_string(RegistriesTags.size()), "TagUtils");
	g_logger->logGameInfo(INFO, "Total tags: " + std::to_string(totalTags), "TagUtils");

	for (const auto& [registryName, tags] : RegistriesTags) {
		size_t totalEntries = 0;
		for (const auto& tag : tags) {
			totalEntries += tag.entries.size();
		}
		g_logger->logGameInfo(
				INFO, registryName + ": " + std::to_string(tags.size()) + " tags, " + std::to_string(totalEntries) + " total entries", "TagUtils");
	}
}
