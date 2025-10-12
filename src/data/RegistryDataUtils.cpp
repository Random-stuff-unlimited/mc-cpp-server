#include "RegistryDataUtils.hpp"

#include "RegistryData.hpp"
#include "RegistryIds.hpp"
#include "minecraftRegistries.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <regex>

std::vector<RegistryData> parseMinecraftRegistries() {
    std::vector<RegistryData> registries;

    try {
        // Reserve space for better performance
        registries.reserve(REGISTRIES.size());

        // Iterate through all registries in the minecraft_registries.h file
        for (const auto& registryPair : REGISTRIES) {
            const std::string& registryName = registryPair.first;
            const Registry& registry = registryPair.second;

            // Skip empty registries
            if (registry.entries.empty()) {
                if (g_logger) {
                    g_logger->logNetwork(WARN, "Skipping empty registry: " + registryName, "RegistryData");
                }
                continue;
            }

            // Create a new RegistryData object
            RegistryData registryData(registryName);

            // Reserve space for entries
            registryData.reserve(registry.entries.size());

            // Add all entries from the registry
            for (const auto& entry : registry.entries) {
                // Add entry without NBT data (has_data = false)
                registryData.addEntry(entry.name, false);
            }

            // Validate the registry before adding it
            if (validateRegistryData(registryData)) {
                registries.push_back(std::move(registryData));

                if (g_logger) {
                    g_logger->logNetwork(INFO,
                        "Parsed registry: " + registryName + " with " +
                        std::to_string(registry.entries.size()) + " entries",
                        "RegistryData");
                }
            } else {
                if (g_logger) {
                    g_logger->logNetwork(ERROR,
                        "Failed to validate parsed registry: " + registryName,
                        "RegistryData");
                }
            }
        }

        if (g_logger) {
            g_logger->logNetwork(INFO,
                "Successfully parsed " + std::to_string(registries.size()) +
                " registries from minecraft_registries.h",
                "RegistryData");
        }

    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR,
                "Failed to parse minecraft registries: " + std::string(e.what()),
                "RegistryData");
        }
        // Return empty vector on error
        registries.clear();
    }

    return registries;
}

void sendRegistryData(Packet& packet, Server& server) {
    g_logger->logNetwork(INFO, "=== Sending Registry Data packets (0x07) using parseMinecraftRegistries ===", "Configuration");

    try {
        // Parse all registries from minecraft_registries.h
        std::vector<RegistryData> registries = parseMinecraftRegistries();

        if (registries.empty()) {
            g_logger->logNetwork(ERROR, "No registries parsed from minecraft_registries.h", "Configuration");
            packet.setReturnPacket(PACKET_ERROR);
            return;
        }

        g_logger->logNetwork(INFO,
            "Successfully parsed " + std::to_string(registries.size()) +
            " registries from minecraft_registries.h",
            "Configuration");

        // Send all registries as a batch using the utility function
        sendRegistryDataBatch(packet, server, registries);

    } catch (const std::exception& e) {
        g_logger->logNetwork(ERROR, "Critical error in Registry Data processing: " + std::string(e.what()), "Configuration");
        packet.setReturnPacket(PACKET_ERROR);
    }
}

bool validateRegistryData(const RegistryData& registry) {
    if (registry.getRegistryId().empty()) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Registry validation failed: empty registry ID", "RegistryData");
        }
        return false;
    }

    if (registry.isEmpty()) {
        if (g_logger) {
            g_logger->logNetwork(WARN, "Registry validation: registry is empty", "RegistryData");
        }
        return true;
    }

    std::set<std::string> seenIds;
    for (const auto& entry : registry.getEntries()) {
        if (entry.entry_id.empty()) {
            if (g_logger) {
                g_logger->logNetwork(ERROR, "Registry validation failed: empty entry ID found", "RegistryData");
            }
            return false;
        }

        if (seenIds.find(entry.entry_id) != seenIds.end()) {
            if (g_logger) {
                g_logger->logNetwork(ERROR, "Registry validation failed: duplicate entry ID: " + entry.entry_id, "RegistryData");
            }
            return false;
        }
        seenIds.insert(entry.entry_id);
    }

    if (g_logger) {
        g_logger->logNetwork(DEBUG, "Registry validation passed: " + registry.toString(), "RegistryData");
    }

    return true;
}

RegistryData createRegistryFromMap(const std::string& registryName, const std::map<std::string, uint32_t>& registryData, bool addNamespace) {
    std::string fullRegistryName = addNamespace ? "minecraft:" + registryName : registryName;
    RegistryData registry(fullRegistryName);

    registry.reserve(registryData.size());

    for (const auto& entry : registryData) {
        registry.addEntry(entry.first, false);
    }

    return registry;
}

std::vector<uint8_t> serializeRegistryPacket(const RegistryData& registry) {
    try {
        // Serialize the registry data first
        std::vector<uint8_t> registryBytes = registry.serialize();

        // Create buffer for the complete packet
        Buffer packetBuffer;

        // Calculate packet size (packet ID + registry data)
        int packetId = RegistryData::PACKET_ID;
        int packetIdSize = 1; // VarInt size for packet ID 0x07
        int totalPayloadSize = packetIdSize + registryBytes.size();

        // Write packet length
        packetBuffer.writeVarInt(totalPayloadSize);

        // Write packet ID
        packetBuffer.writeVarInt(packetId);

        // Write registry data
        packetBuffer.writeBytes(registryBytes);

        return packetBuffer.getData();

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to serialize registry packet: " + std::string(e.what()));
    }
}

bool deserializeRegistryPacket(const std::vector<uint8_t>& packetData, RegistryData& outRegistry) {
    try {
        std::vector<uint8_t> dataCopy = packetData;
        Buffer buffer(dataCopy);

        // Read packet length (but we don't need to use it for validation)
        buffer.readVarInt();

        // Read packet ID
        int32_t packetId = buffer.readVarInt();

        // Verify packet ID
        if (packetId != RegistryData::PACKET_ID) {
            if (g_logger) {
                g_logger->logNetwork(ERROR,
                    "Invalid packet ID for registry data: expected " + std::to_string(RegistryData::PACKET_ID) + ", got " +
                    std::to_string(packetId),
                    "RegistryData");
            }
            return false;
        }

        // Calculate remaining data size for registry content
        size_t remainingBytes = buffer.remaining();
        std::vector<uint8_t> registryData(buffer.getData().begin() + (buffer.getData().size() - remainingBytes), buffer.getData().end());

        // Deserialize the registry data
        return outRegistry.deserialize(registryData);

    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Failed to deserialize registry packet: " + std::string(e.what()), "RegistryData");
        }
        return false;
    }
}

std::vector<std::string> getEssentialRegistryNames() {
    return {"block",
            "item",
            "entity_type",
            "sound_event",
            "particle_type",
            "mob_effect",
            "block_entity_type",
            "menu",
            "recipe_type",
            "recipe_serializer"};
}

size_t estimateRegistrySize(const RegistryData& registry) {
    size_t estimatedSize = 0;

    // Registry ID string (VarInt length + string bytes + null terminator estimate)
    estimatedSize += 5 + registry.getRegistryId().length(); // VarInt up to 5 bytes for length

    // Entry count (VarInt)
    estimatedSize += 5; // VarInt up to 5 bytes

    // Entries
    for (const auto& entry : registry.getEntries()) {
        // Entry ID string
        estimatedSize += 5 + entry.entry_id.length(); // VarInt length + string

        // Has data boolean
        estimatedSize += 1;

        // NBT data if present (rough estimate)
        if (entry.has_data && entry.data.has_value()) {
            estimatedSize += 100; // Rough estimate for NBT data
        }
    }

    return estimatedSize;
}

RegistryData createTestRegistry(const std::string& registryName, size_t entryCount) {
    RegistryData testRegistry("minecraft:" + registryName);
    testRegistry.reserve(entryCount);

    for (size_t i = 0; i < entryCount; ++i) {
        std::string entryId = "minecraft:test_entry_" + std::to_string(i);
        testRegistry.addEntry(entryId, false);
    }

    return testRegistry;
}

bool compareRegistries(const RegistryData& reg1, const RegistryData& reg2) {
    if (reg1.getRegistryId() != reg2.getRegistryId()) {
        return false;
    }

    if (reg1.getEntryCount() != reg2.getEntryCount()) {
        return false;
    }

    const auto& entries1 = reg1.getEntries();
    const auto& entries2 = reg2.getEntries();

    for (size_t i = 0; i < entries1.size(); ++i) {
        const auto& entry1 = entries1[i];
        const auto& entry2 = entries2[i];

        if (entry1.entry_id != entry2.entry_id || entry1.has_data != entry2.has_data) {
            return false;
        }
    }

    return true;
}

RegistryData mergeRegistries(const std::vector<RegistryData>& registries, const std::string& mergedRegistryName) {
    RegistryData merged(mergedRegistryName);

    // Calculate total capacity needed
    size_t totalEntries = 0;
    for (const auto& registry : registries) {
        totalEntries += registry.getEntryCount();
    }
    merged.reserve(totalEntries);

    // Merge entries from all registries
    for (const auto& registry : registries) {
        // Extract base registry name for prefixing
        std::string baseRegistryName = registry.getRegistryId();
        size_t colonPos = baseRegistryName.find(':');
        if (colonPos != std::string::npos && colonPos < baseRegistryName.length() - 1) {
            baseRegistryName = baseRegistryName.substr(colonPos + 1);
        }

        for (const auto& entry : registry.getEntries()) {
            // Prefix entry ID with registry name to avoid conflicts
            std::string prefixedId = baseRegistryName + ":" + entry.entry_id;
            merged.addEntry(prefixedId, entry.has_data, entry.data);
        }
    }

    return merged;
}

void sendRegistryDataBatch(Packet& packet, Server& server, const std::vector<RegistryData>& registries) {
    if (registries.empty()) {
        if (g_logger) {
            g_logger->logNetwork(WARN, "No registries provided to sendRegistryDataBatch", "Configuration");
        }
        packet.setReturnPacket(PACKET_OK);
        return;
    }

    Player* player = packet.getPlayer();
    if (!player) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "No player associated with Registry Data batch", "Configuration");
        }
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    ThreadSafeQueue<Packet*>* outgoingPackets = server.getNetworkManager().getOutgoingQueue();
    if (!outgoingPackets) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "No outgoing packet queue available", "Configuration");
        }
        packet.setReturnPacket(PACKET_ERROR);
        return;
    }

    int successfulPackets = 0;

    for (const auto& registry : registries) {
        if (!validateRegistryData(registry)) {
            if (g_logger) {
                g_logger->logNetwork(ERROR, "Invalid registry data for: " + registry.getRegistryId(), "Configuration");
            }
            continue;
        }

        try {
            // Serialize the registry packet
            std::vector<uint8_t> packetData = serializeRegistryPacket(registry);

            // Create new packet
            Packet* registryPacket = new Packet(packet);

            Buffer buffer;
            buffer.writeBytes(packetData);

            registryPacket->getData() = buffer;
            registryPacket->setPacketSize(packetData.size());
            registryPacket->setReturnPacket(PACKET_SEND);

            // Queue the packet
            outgoingPackets->push(registryPacket);
            successfulPackets++;

            if (g_logger) {
                g_logger->logNetwork(INFO,
                    "Registry packet queued for '" + registry.getRegistryId() + "' with " +
                    std::to_string(registry.getEntryCount()) + " entries, size: " + std::to_string(packetData.size()) +
                    " bytes",
                    "Configuration");
            }

        } catch (const std::exception& e) {
            if (g_logger) {
                g_logger->logNetwork(
                    ERROR, "Failed to create packet for registry '" + registry.getRegistryId() + "': " + std::string(e.what()), "Configuration");
            }
        }
    }

    if (g_logger) {
        g_logger->logNetwork(INFO,
            "Registry batch processing complete: " + std::to_string(successfulPackets) + "/" + std::to_string(registries.size()) +
            " packets sent",
            "Configuration");
    }

    packet.setReturnPacket(successfulPackets > 0 ? PACKET_OK : PACKET_ERROR);
}

std::vector<RegistryData> createAllEssentialRegistries() {
    std::vector<RegistryData> registries;

    try {
        // Primary method: use parseMinecraftRegistries
        registries = parseMinecraftRegistries();

        if (!registries.empty()) {
            if (g_logger) {
                g_logger->logNetwork(INFO, "Using parsed registries from minecraft_registries.h", "RegistryData");
            }
            return registries;
        }

        // Fallback method: use RegistryIds
        if (g_logger) {
            g_logger->logNetwork(WARN, "Falling back to legacy RegistryIds method", "RegistryData");
        }

        RegistryIds registryIds;

        std::vector<std::pair<std::string, std::map<std::string, uint32_t>>> registryDataSources = {
            {"block", registryIds.getBlock()},
            {"item", registryIds.getItem()},
            {"entity_type", registryIds.getEntityType()},
            {"sound_event", registryIds.getSoundEvent()},
            {"particle_type", registryIds.getParticleType()},
            {"mob_effect", registryIds.getMobEffect()},
            {"block_entity_type", registryIds.getBlockEntityType()},
            {"menu", registryIds.getMenu()},
            {"recipe_type", registryIds.getRecipeType()},
            {"recipe_serializer", registryIds.getRecipeSerializer()}
        };

        registries.reserve(registryDataSources.size());

        for (const auto& source : registryDataSources) {
            if (!source.second.empty()) {
                RegistryData registry = createRegistryFromMap(source.first, source.second, true);
                registries.push_back(std::move(registry));
            } else {
                if (g_logger) {
                    g_logger->logNetwork(WARN, "Skipping empty registry: " + source.first, "RegistryData");
                }
            }
        }
    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Failed to create essential registries: " + std::string(e.what()), "RegistryData");
        }
    }

    return registries;
}

bool saveRegistryToFile(const RegistryData& registry, const std::string& filename) {
    try {
        std::vector<uint8_t> data = registry.serialize();

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            if (g_logger) {
                g_logger->logNetwork(ERROR, "Failed to open file for writing: " + filename, "RegistryData");
            }
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();

        if (g_logger) {
            g_logger->logNetwork(INFO, "Registry saved to file: " + filename, "RegistryData");
        }

        return true;

    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Failed to save registry to file: " + std::string(e.what()), "RegistryData");
        }
        return false;
    }
}

bool loadRegistryFromFile(RegistryData& registry, const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            if (g_logger) {
                g_logger->logNetwork(ERROR, "Failed to open file for reading: " + filename, "RegistryData");
            }
            return false;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read file data
        std::vector<uint8_t> data(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        file.close();

        // Deserialize
        bool success = registry.deserialize(data);

        if (success && g_logger) {
            g_logger->logNetwork(INFO, "Registry loaded from file: " + filename, "RegistryData");
        }

        return success;

    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Failed to load registry from file: " + std::string(e.what()), "RegistryData");
        }
        return false;
    }
}

RegistryData createRegistryFromEntries(const std::string& registryName, const std::vector<RegistryEntry>& registryEntries) {
    RegistryData registry(registryName);
    registry.reserve(registryEntries.size());

    for (const auto& entry : registryEntries) {
        registry.addEntry(entry.name, false);
    }

    return registry;
}

std::vector<RegistryData> filterRegistriesByName(const std::vector<RegistryData>& registries, const std::string& pattern, bool useRegex) {
    std::vector<RegistryData> filtered;

    try {
        if (useRegex) {
            std::regex regexPattern(pattern);
            for (const auto& registry : registries) {
                if (std::regex_search(registry.getRegistryId(), regexPattern)) {
                    filtered.push_back(registry);
                }
            }
        } else {
            for (const auto& registry : registries) {
                if (registry.getRegistryId().find(pattern) != std::string::npos) {
                    filtered.push_back(registry);
                }
            }
        }
    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->logNetwork(ERROR, "Failed to filter registries: " + std::string(e.what()), "RegistryData");
        }
    }

    return filtered;
}

std::map<std::string, size_t> getRegistryStatistics(const std::vector<RegistryData>& registries) {
    std::map<std::string, size_t> stats;

    stats["total_registries"] = registries.size();

    size_t totalEntries = 0;
    size_t minEntries = SIZE_MAX;
    size_t maxEntries = 0;

    for (const auto& registry : registries) {
        size_t entryCount = registry.getEntryCount();
        totalEntries += entryCount;

        if (entryCount < minEntries) minEntries = entryCount;
        if (entryCount > maxEntries) maxEntries = entryCount;
    }

    stats["total_entries"] = totalEntries;
    stats["min_entries_per_registry"] = registries.empty() ? 0 : minEntries;
    stats["max_entries_per_registry"] = maxEntries;
    stats["avg_entries_per_registry"] = registries.empty() ? 0 : totalEntries / registries.size();

    return stats;
}

std::string registryToString(const RegistryData& registry, bool includeEntries, size_t maxEntries) {
    std::string result = "Registry: " + registry.getRegistryId() + " (" + std::to_string(registry.getEntryCount()) + " entries)";

    if (includeEntries && !registry.isEmpty()) {
        result += "\nEntries:";
        const auto& entries = registry.getEntries();
        size_t count = 0;

        for (const auto& entry : entries) {
            if (maxEntries > 0 && count >= maxEntries) {
                result += "\n  ... and " + std::to_string(entries.size() - count) + " more entries";
                break;
            }
            result += "\n  - " + entry.entry_id + (entry.has_data ? " (with data)" : "");
            count++;
        }
    }

    return result;
}

RegistryData createMinimalRegistry(const std::string& registryName, const std::vector<std::string>& entryNames) {
    RegistryData registry(registryName);
    registry.reserve(entryNames.size());

    for (const auto& entryName : entryNames) {
        registry.addEntry(entryName, false);
    }

    return registry;
}

bool validateAllRegistries(const std::vector<RegistryData>& registries) {
    for (const auto& registry : registries) {
        if (!validateRegistryData(registry)) {
            return false;
        }
    }
    return true;
}

void sortRegistries(std::vector<RegistryData>& registries, const std::string& sortBy, bool ascending) {
    if (sortBy == "name") {
        std::sort(registries.begin(), registries.end(),
            [ascending](const RegistryData& a, const RegistryData& b) {
                return ascending ? a.getRegistryId() < b.getRegistryId() : a.getRegistryId() > b.getRegistryId();
            });
    } else if (sortBy == "size" || sortBy == "entries") {
        std::sort(registries.begin(), registries.end(),
            [ascending](const RegistryData& a, const RegistryData& b) {
                return ascending ? a.getEntryCount() < b.getEntryCount() : a.getEntryCount() > b.getEntryCount();
            });
    }
}

const RegistryData* findRegistryByName(const std::vector<RegistryData>& registries, const std::string& registryName) {
    for (const auto& registry : registries) {
        if (registry.getRegistryId() == registryName) {
            return &registry;
        }
    }
    return nullptr;
}

std::vector<RegistryData> backupRegistries(const std::vector<RegistryData>& registries) {
    // Deep copy by creating new vector with copy constructor
    return std::vector<RegistryData>(registries);
}
