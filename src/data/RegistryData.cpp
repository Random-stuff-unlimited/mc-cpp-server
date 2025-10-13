#include "data/RegistryData.hpp"
#include "network/buffer.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

RegistryData::RegistryData() : _registry_id(""), _entries() {}

RegistryData::RegistryData(const std::string& registryId) : _registry_id(registryId), _entries() {}

RegistryData::~RegistryData() = default;

RegistryData::RegistryData(const RegistryData& other) : _registry_id(other._registry_id), _entries(other._entries) {}

RegistryData& RegistryData::operator=(const RegistryData& other) {
	if (this != &other) {
		_registry_id = other._registry_id;
		_entries	 = other._entries;
	}
	return *this;
}

RegistryData::RegistryData(RegistryData&& other) noexcept : _registry_id(std::move(other._registry_id)), _entries(std::move(other._entries)) {}

RegistryData& RegistryData::operator=(RegistryData&& other) noexcept {
	if (this != &other) {
		_registry_id = std::move(other._registry_id);
		_entries	 = std::move(other._entries);
	}
	return *this;
}

const std::string& RegistryData::getRegistryId() const { return _registry_id; }

const std::vector<RegistryDataEntry>& RegistryData::getEntries() const { return _entries; }

size_t RegistryData::getEntryCount() const { return _entries.size(); }

void RegistryData::addEntry(const std::string& entryId, bool hasData, std::optional<std::shared_ptr<nbt::Tag>> data) {
	_entries.emplace_back(entryId, hasData, data);
}

std::vector<uint8_t> RegistryData::serialize() const {
	try {
		Buffer buffer;

		// Format MC 1.21.5: id + entries length + entries array
		buffer.writeIdentifier(_registry_id);
		buffer.writeVarInt(static_cast<int32_t>(_entries.size()));

		for (const auto& entry : _entries) {
			// Chaque entrée: key (string) + value optional (anonymousNbt)
			buffer.writeIdentifier(entry.entry_id);

			// Format "value optional": boolean présent + données NBT si présent
			if (entry.has_data && entry.data.has_value()) {
				buffer.writeBool(true); // Données présentes
				buffer.writeNBT("{}");	// Données NBT (vide pour l'instant)
			} else {
				buffer.writeBool(false); // Pas de données NBT optionnelles
			}
		}

		return buffer.getData();

	} catch (const std::exception& e) {
		throw std::runtime_error("Failed to serialize RegistryData: " + std::string(e.what()));
	}
}

bool RegistryData::isEmpty() const { return _entries.empty(); }

void RegistryData::reserve(size_t capacity) { _entries.reserve(capacity); }
