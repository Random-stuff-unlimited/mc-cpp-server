#ifndef MC_CPP_SERVER_DATA_REGISTRY_HPP
#define MC_CPP_SERVER_DATA_REGISTRY_HPP

#include "nbt.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct RegistryDataEntry {
	std::string								 entry_id;
	bool									 has_data;
	std::optional<std::shared_ptr<nbt::Tag>> data;

	RegistryDataEntry() : has_data(false) {}

	RegistryDataEntry(const std::string& id, bool hasData = false, std::optional<std::shared_ptr<nbt::Tag>> nbtData = std::nullopt)
		: entry_id(id), has_data(hasData), data(nbtData) {}
};

class RegistryData {
  private:
	std::string					   _registry_id;
	std::vector<RegistryDataEntry> _entries;

  public:
	RegistryData();
	RegistryData(const std::string& registryId);

	~RegistryData();

	RegistryData(const RegistryData& other);
	RegistryData& operator=(const RegistryData& other);

	RegistryData(RegistryData&& other) noexcept;
	RegistryData& operator=(RegistryData&& other) noexcept;

	const std::string&					  getRegistryId() const;
	const std::vector<RegistryDataEntry>& getEntries() const;
	size_t								  getEntryCount() const;

	void addEntry(const std::string& entryId, bool hasData = false, std::optional<std::shared_ptr<nbt::Tag>> data = std::nullopt);

	std::vector<uint8_t> serialize() const;

	static constexpr uint8_t PACKET_ID = 0x07;

	bool isEmpty() const;
	void reserve(size_t capacity);
};

#endif
