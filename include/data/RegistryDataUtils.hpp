#ifndef MC_CPP_SERVER_DATA_REGISTRY_UTILS_HPP
#define MC_CPP_SERVER_DATA_REGISTRY_UTILS_HPP

#include "RegistryData.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "minecraftRegistries.hpp"

#include <vector>
#include <string>
#include <map>

/**
 * Utility functions for handling Registry Data packets and operations
 */

/**
 * Parses the minecraft_registries.h file data into a vector of RegistryData objects.
 * This function processes the MINECRAFT_REGISTRIES map and converts each registry
 * into a RegistryData object suitable for network transmission.
 *
 * @return Vector of RegistryData objects parsed from minecraft_registries.h
 */
std::vector<RegistryData> parseMinecraftRegistries();

/**
 * Sends Registry Data packet (0x07) to client during configuration phase.
 * This function creates and sends separate packets for each essential registry.
 *
 * @param packet The incoming packet object
 * @param server The server instance
 */
void sendRegistryData(Packet& packet, Server& server);

/**
 * Creates a combined RegistryData object containing all essential registries.
 * This can be used as an alternative to sending separate packets for each registry.
 *
 * @return RegistryData object with all combined registry entries
 */
RegistryData createCombinedRegistryData();

/**
 * Validates a RegistryData object for correctness and consistency.
 * Checks for empty registry IDs, duplicate entry IDs, and other validation rules.
 *
 * @param registry The RegistryData object to validate
 * @return true if the registry is valid, false otherwise
 */
bool validateRegistryData(const RegistryData& registry);

/**
 * Creates a RegistryData object from a specific registry type and data map.
 *
 * @param registryName The name of the registry (e.g., "block", "item")
 * @param registryData Map of entry names to their IDs
 * @param addNamespace Whether to add "minecraft:" namespace to the registry ID
 * @return RegistryData object populated with the provided data
 */
RegistryData createRegistryFromMap(const std::string& registryName, const std::map<std::string, uint32_t>& registryData, bool addNamespace = true);

/**
 * Serializes a RegistryData object into a complete packet buffer ready to send.
 * Includes packet length, packet ID, and serialized registry data.
 *
 * @param registry The RegistryData object to serialize
 * @return Vector of bytes representing the complete packet
 */
std::vector<uint8_t> serializeRegistryPacket(const RegistryData& registry);

/**
 * Deserializes a complete registry packet buffer into a RegistryData object.
 * Expects the buffer to contain packet length, packet ID, and registry data.
 *
 * @param packetData The complete packet data
 * @param outRegistry Reference to RegistryData object to populate
 * @return true if deserialization was successful, false otherwise
 */
bool deserializeRegistryPacket(const std::vector<uint8_t>& packetData, RegistryData& outRegistry);

/**
 * Gets the list of essential registry names that should be sent to clients.
 *
 * @return Vector of registry names
 */
std::vector<std::string> getEssentialRegistryNames();

/**
 * Estimates the serialized size of a RegistryData object without actually serializing it.
 * Useful for memory allocation and packet size planning.
 *
 * @param registry The RegistryData object to estimate
 * @return Estimated size in bytes
 */
size_t estimateRegistrySize(const RegistryData& registry);

/**
 * Creates a minimal test RegistryData object for testing purposes.
 *
 * @param registryName Name of the test registry
 * @param entryCount Number of test entries to create
 * @return RegistryData object with test data
 */
RegistryData createTestRegistry(const std::string& registryName, size_t entryCount);

/**
 * Compares two RegistryData objects for equality.
 *
 * @param reg1 First registry to compare
 * @param reg2 Second registry to compare
 * @return true if registries are equal, false otherwise
 */
bool compareRegistries(const RegistryData& reg1, const RegistryData& reg2);

/**
 * Merges multiple RegistryData objects into a single registry.
 * Entry IDs will be prefixed with their original registry names to avoid conflicts.
 *
 * @param registries Vector of RegistryData objects to merge
 * @param mergedRegistryName Name for the merged registry
 * @return RegistryData object containing all merged entries
 */
RegistryData mergeRegistries(const std::vector<RegistryData>& registries, const std::string& mergedRegistryName);

/**
 * Sends multiple RegistryData objects as separate packets in a batch.
 *
 * @param packet The incoming packet object
 * @param server The server instance
 * @param registries Vector of RegistryData objects to send
 */
void sendRegistryDataBatch(Packet& packet, Server& server, const std::vector<RegistryData>& registries);

/**
 * Creates all essential registries and returns them as a vector.
 * Uses parseMinecraftRegistries() as the primary method, with fallback to legacy method.
 *
 * @return Vector of RegistryData objects for all essential registries
 */
std::vector<RegistryData> createAllEssentialRegistries();

/**
 * Saves a RegistryData object to a binary file.
 *
 * @param registry The RegistryData object to save
 * @param filename Path to the output file
 * @return true if successful, false otherwise
 */
bool saveRegistryToFile(const RegistryData& registry, const std::string& filename);

/**
 * Loads a RegistryData object from a binary file.
 *
 * @param registry Reference to RegistryData object to populate
 * @param filename Path to the input file
 * @return true if successful, false otherwise
 */
bool loadRegistryFromFile(RegistryData& registry, const std::string& filename);

/**
 * Creates a registry data object from parsed minecraft_registries.h entry.
 *
 * @param registryName The registry name/ID
 * @param registryEntries Vector of registry entries
 * @return RegistryData object populated with the entries
 */
RegistryData createRegistryFromEntries(const std::string& registryName, const std::vector<RegistryEntry>& registryEntries);

/**
 * Filters registries by name pattern.
 *
 * @param registries Vector of RegistryData objects to filter
 * @param pattern Pattern to match against registry names
 * @param useRegex Whether to use regex matching (default: false, uses simple contains)
 * @return Vector of matching RegistryData objects
 */
std::vector<RegistryData> filterRegistriesByName(const std::vector<RegistryData>& registries, const std::string& pattern, bool useRegex = false);

/**
 * Gets statistics about a collection of registries.
 *
 * @param registries Vector of RegistryData objects
 * @return Map containing statistics (total_registries, total_entries, avg_entries_per_registry, etc.)
 */
std::map<std::string, size_t> getRegistryStatistics(const std::vector<RegistryData>& registries);

/**
 * Converts a RegistryData object to a human-readable string representation.
 *
 * @param registry The RegistryData object to convert
 * @param includeEntries Whether to include all entries in the output
 * @param maxEntries Maximum number of entries to include (0 = all)
 * @return String representation of the registry
 */
std::string registryToString(const RegistryData& registry, bool includeEntries = false, size_t maxEntries = 10);

/**
 * Creates a minimal registry with only specified entries.
 *
 * @param registryName Name of the registry
 * @param entryNames Vector of entry names to include
 * @return RegistryData object with only the specified entries
 */
RegistryData createMinimalRegistry(const std::string& registryName, const std::vector<std::string>& entryNames);

/**
 * Validates that all registries in a vector are consistent and valid.
 *
 * @param registries Vector of RegistryData objects to validate
 * @return true if all registries are valid, false otherwise
 */
bool validateAllRegistries(const std::vector<RegistryData>& registries);

/**
 * Sorts registries by various criteria.
 *
 * @param registries Vector of RegistryData objects to sort (modified in place)
 * @param sortBy Sorting criteria: "name", "size", "entries"
 * @param ascending Whether to sort in ascending order
 */
void sortRegistries(std::vector<RegistryData>& registries, const std::string& sortBy = "name", bool ascending = true);

/**
 * Finds a specific registry by name in a vector.
 *
 * @param registries Vector of RegistryData objects to search
 * @param registryName Name of the registry to find
 * @return Pointer to the found registry, or nullptr if not found
 */
const RegistryData* findRegistryByName(const std::vector<RegistryData>& registries, const std::string& registryName);

/**
 * Creates a backup copy of all registries.
 *
 * @param registries Vector of RegistryData objects to backup
 * @return Deep copy of all registries
 */
std::vector<RegistryData> backupRegistries(const std::vector<RegistryData>& registries);

#endif // MC_CPP_SERVER_DATA_REGISTRY_UTILS_HPP
