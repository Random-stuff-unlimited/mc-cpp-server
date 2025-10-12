#include "data/RegistryData.hpp"
#include "network/buffer.hpp"
#include "logger.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <string>
#include <vector>

RegistryData::RegistryData() : _registry_id(""), _entries() {}

RegistryData::RegistryData(const std::string& registryId)
    : _registry_id(registryId), _entries() {}

RegistryData::RegistryData(const std::string& registryId, const std::vector<RegistryDataEntry>& entries)
    : _registry_id(registryId), _entries(entries) {}

RegistryData::~RegistryData() = default;

RegistryData::RegistryData(const RegistryData& other)
    : _registry_id(other._registry_id), _entries(other._entries) {}

RegistryData& RegistryData::operator=(const RegistryData& other) {
    if (this != &other) {
        _registry_id = other._registry_id;
        _entries = other._entries;
    }
    return *this;
}

RegistryData::RegistryData(RegistryData&& other) noexcept
    : _registry_id(std::move(other._registry_id)), _entries(std::move(other._entries)) {}

RegistryData& RegistryData::operator=(RegistryData&& other) noexcept {
    if (this != &other) {
        _registry_id = std::move(other._registry_id);
        _entries = std::move(other._entries);
    }
    return *this;
}

const std::string& RegistryData::getRegistryId() const {
    return _registry_id;
}

const std::vector<RegistryDataEntry>& RegistryData::getEntries() const {
    return _entries;
}

size_t RegistryData::getEntryCount() const {
    return _entries.size();
}

void RegistryData::setRegistryId(const std::string& registryId) {
    _registry_id = registryId;
}

void RegistryData::setEntries(const std::vector<RegistryDataEntry>& entries) {
    _entries = entries;
}

void RegistryData::addEntry(const RegistryDataEntry& entry) {
    _entries.push_back(entry);
}

void RegistryData::addEntry(const std::string& entryId, bool hasData,
                           std::optional<std::shared_ptr<nbt::Tag>> data) {
    _entries.emplace_back(entryId, hasData, data);
}

void RegistryData::clearEntries() {
    _entries.clear();
}

bool RegistryData::removeEntry(const std::string& entryId) {
    auto it = std::find_if(_entries.begin(), _entries.end(),
        [&entryId](const RegistryDataEntry& entry) {
            return entry.entry_id == entryId;
        });

    if (it != _entries.end()) {
        _entries.erase(it);
        return true;
    }
    return false;
}

const RegistryDataEntry* RegistryData::findEntry(const std::string& entryId) const {
    auto it = std::find_if(_entries.begin(), _entries.end(),
        [&entryId](const RegistryDataEntry& entry) {
            return entry.entry_id == entryId;
        });

    return (it != _entries.end()) ? &(*it) : nullptr;
}

RegistryDataEntry* RegistryData::findEntry(const std::string& entryId) {
    auto it = std::find_if(_entries.begin(), _entries.end(),
        [&entryId](const RegistryDataEntry& entry) {
            return entry.entry_id == entryId;
        });

    return (it != _entries.end()) ? &(*it) : nullptr;
}

std::vector<uint8_t> RegistryData::serialize() const {
    try {
        Buffer buffer;

        // Write registry identifier
        buffer.writeIdentifier(_registry_id);

        // Write number of entries
        buffer.writeVarInt(static_cast<int32_t>(_entries.size()));

        // Write each entry
        for (const auto& entry : _entries) {
            // Write entry identifier
            buffer.writeIdentifier(entry.entry_id);

            // Write has_data flag
            buffer.writeBool(entry.has_data);

            // If has_data is true, write the NBT data
            if (entry.has_data && entry.data.has_value()) {
                // Note: This assumes we have a method to serialize NBT data
                // You'll need to implement NBT serialization based on your NBT implementation
                // For now, this is a placeholder
                buffer.writeNBT("{}"); // Placeholder - replace with actual NBT serialization
            }
        }

        return buffer.getData();

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to serialize RegistryData: " + std::string(e.what()));
    }
}

// Deserialization method - creates RegistryData from raw bytes (non-consuming)
bool RegistryData::deserialize(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> dataCopy = data;
    return deserialize(dataCopy);
}

// Deserialization method - creates RegistryData from raw bytes (consuming)
bool RegistryData::deserialize(std::vector<uint8_t>& data) {
    try {
        Buffer buffer(data);

        // Read registry identifier
        _registry_id = buffer.readString(32767); // Max string length for identifiers

        // Read number of entries
        int32_t entryCount = buffer.readVarInt();

        if (entryCount < 0) {
            return false; // Invalid entry count
        }

        // Clear existing entries and reserve space
        _entries.clear();
        _entries.reserve(static_cast<size_t>(entryCount));

        // Read each entry
        for (int32_t i = 0; i < entryCount; ++i) {
            RegistryDataEntry entry;

            // Read entry identifier
            entry.entry_id = buffer.readString(32767);

            // Read has_data flag
            entry.has_data = (buffer.readByte() != 0);

            // If has_data is true, read the NBT data
            if (entry.has_data) {
                // Note: This is a placeholder for NBT deserialization
                // You'll need to implement this based on your NBT implementation
                // For now, we'll skip the NBT data or set it to null
                entry.data = std::nullopt; // Placeholder
            } else {
                entry.data = std::nullopt;
            }

            _entries.push_back(std::move(entry));
        }

        return true;

    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Failed to deserialize RegistryData: " + std::string(e.what()), "RegistryData");
        }
        return false;
    }
}

bool RegistryData::isEmpty() const {
    return _entries.empty();
}

void RegistryData::reserve(size_t capacity) {
    _entries.reserve(capacity);
}

std::string RegistryData::toString() const {
    std::ostringstream oss;
    oss << "RegistryData{";
    oss << "registry_id: '" << _registry_id << "', ";
    oss << "entries: [";

    for (size_t i = 0; i < _entries.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "{";
        oss << "id: '" << _entries[i].entry_id << "', ";
        oss << "has_data: " << (_entries[i].has_data ? "true" : "false");
        oss << "}";
    }

    oss << "] (count: " << _entries.size() << ")}";
    return oss.str();
}
