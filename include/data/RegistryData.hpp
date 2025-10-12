#ifndef MC_CPP_SERVER_DATA_REGISTRY_HPP
#define MC_CPP_SERVER_DATA_REGISTRY_HPP

#include "nbt.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>

struct RegistryDataEntry {
    std::string entry_id;
    bool has_data;
    std::optional<std::shared_ptr<nbt::Tag>> data;

    RegistryDataEntry() : has_data(false) {}

    RegistryDataEntry(const std::string& id, bool hasData = false,
                     std::optional<std::shared_ptr<nbt::Tag>> nbtData = std::nullopt)
        : entry_id(id), has_data(hasData), data(nbtData) {}
};

class RegistryData {
    private:
        std::string _registry_id;
        std::vector<RegistryDataEntry> _entries;

    public:
        // Constructors
        RegistryData();
        RegistryData(const std::string& registryId);
        RegistryData(const std::string& registryId, const std::vector<RegistryDataEntry>& entries);

        // Destructor
        ~RegistryData();

        // Copy constructor and assignment operator
        RegistryData(const RegistryData& other);
        RegistryData& operator=(const RegistryData& other);

        // Move constructor and assignment operator
        RegistryData(RegistryData&& other) noexcept;
        RegistryData& operator=(RegistryData&& other) noexcept;

        // Getters
        const std::string& getRegistryId() const;
        const std::vector<RegistryDataEntry>& getEntries() const;
        size_t getEntryCount() const;

        // Setters
        void setRegistryId(const std::string& registryId);
        void setEntries(const std::vector<RegistryDataEntry>& entries);

        // Entry management
        void addEntry(const RegistryDataEntry& entry);
        void addEntry(const std::string& entryId, bool hasData = false,
                    std::optional<std::shared_ptr<nbt::Tag>> data = std::nullopt);
        void clearEntries();
        bool removeEntry(const std::string& entryId);

        // Find entry by ID
        const RegistryDataEntry* findEntry(const std::string& entryId) const;
        RegistryDataEntry* findEntry(const std::string& entryId);

        // Serialization methods
        std::vector<uint8_t> serialize() const;
        bool deserialize(const std::vector<uint8_t>& data);
        bool deserialize(std::vector<uint8_t>& data); // For consuming data

        // Packet constants
        static constexpr uint8_t PACKET_ID = 0x07;

        // Utility methods
        bool isEmpty() const;
        void reserve(size_t capacity);

        // Debug/logging
        std::string toString() const;
};

#endif
