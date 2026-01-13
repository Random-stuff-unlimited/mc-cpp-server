#include "data/RegistryDataUtils.hpp"
#include "data/RegistryData.hpp"
#include "data/RegistryIds.hpp"
#include "data/minecraftRegistries.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/PacketIds.hpp"
#include "network/networking.hpp"
#include "logger.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

// ============================================================
// ==========         REGISTRY VALIDATION         ==========
// ============================================================

bool validateRegistryData(const RegistryData& registry) {
	if (registry.getRegistryId().empty()) {
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Error, "Registry validation failed: empty registry ID", "RegistryData");
		}
		return false;
	}

	if (registry.isEmpty()) {
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Warn, "Registry is empty: " + registry.getRegistryId(), "RegistryData");
		}
		return true; // Empty registries are valid in MC 1.21.5
	}

	// Check for duplicate entry IDs
	std::set<std::string> entryIds;
	for (const auto& entry : registry.getEntries()) {
		if (entry.entry_id.empty()) {
			if (g_logger) {
				g_logger->logNetwork(LogLevel::Error, "Entry with empty ID in registry: " + registry.getRegistryId(), "RegistryData");
			}
			return false;
		}

		if (!entryIds.insert(entry.entry_id).second) {
			if (g_logger) {
				g_logger->logNetwork(LogLevel::Error, "Duplicate entry ID '" + entry.entry_id + "' in registry: " + registry.getRegistryId(), "RegistryData");
			}
			return false;
		}
	}

	return true;
}

// ============================================================
// ==========         REGISTRY PARSING            ==========
// ============================================================

static bool requiresNBT(const std::string& registryName) {
	return registryName == "minecraft:damage_type" ||
	       registryName == "minecraft:dimension_type" ||
	       registryName == "minecraft:enchantment" ||
	       registryName == "minecraft:worldgen/biome" ||
	       registryName == "minecraft:banner_pattern" ||
	       registryName == "minecraft:trim_material" ||
	       registryName == "minecraft:trim_pattern" ||
	       registryName == "minecraft:painting_variant" ||
	       registryName == "minecraft:jukebox_song" ||
	       registryName == "minecraft:instrument" ||
	       registryName == "minecraft:chat_type";
}

std::vector<RegistryData> parseMinecraftRegistries() {
	std::vector<RegistryData> registries;

	try {
		registries.reserve(REGISTRIES.size());

		for (const auto& [registryName, registry] : REGISTRIES) {
			RegistryData registryData(registryName);

			if (registry.entries.empty()) {
				if (g_logger) {
					g_logger->logNetwork(LogLevel::Warn, "Creating empty registry: " + registryName, "RegistryData");
				}
				registries.push_back(std::move(registryData));
				continue;
			}

			registryData.reserve(registry.entries.size());

			bool needsNBT = requiresNBT(registryName);
			for (const auto& entry : registry.entries) {
				registryData.addEntry(entry.name, needsNBT);
			}

			if (validateRegistryData(registryData)) {
				registries.push_back(std::move(registryData));

				if (g_logger) {
					g_logger->logNetwork(LogLevel::Info, "Parsed registry: " + registryName + " (" +
					                     std::to_string(registry.entries.size()) + " entries)", "RegistryData");
				}
			} else {
				if (g_logger) {
					g_logger->logNetwork(LogLevel::Error, "Failed to validate registry: " + registryName, "RegistryData");
				}
			}
		}

		if (g_logger) {
			g_logger->logNetwork(LogLevel::Info, "Successfully parsed " + std::to_string(registries.size()) +
			                     " registries from minecraft_registries.h", "RegistryData");
		}

	} catch (const std::exception& e) {
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Error, "Failed to parse minecraft registries: " + std::string(e.what()), "RegistryData");
		}
		registries.clear();
	}

	return registries;
}

// ============================================================
// ==========         REGISTRY CREATION           ==========
// ============================================================

RegistryData createRegistryFromMap(const std::string& registryName,
                                   const std::map<std::string, uint32_t>& registryData,
                                   bool addNamespace) {
	std::string fullRegistryName = addNamespace ? "minecraft:" + registryName : registryName;
	RegistryData registry(fullRegistryName);

	registry.reserve(registryData.size());

	for (const auto& [entryName, _] : registryData) {
		registry.addEntry(entryName, false);
	}

	return registry;
}

std::vector<RegistryData> createAllEssentialRegistries() {
	std::vector<RegistryData> registries;

	try {
		registries = parseMinecraftRegistries();

		if (!registries.empty()) {
			if (g_logger) {
				g_logger->logNetwork(LogLevel::Info, "Using parsed registries from minecraft_registries.h", "RegistryData");
			}
			return registries;
		}

		// Fallback method
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Warn, "parseMinecraftRegistries returned empty, using fallback", "RegistryData");
		}

		std::vector<std::pair<std::string, std::map<std::string, uint32_t>>> sources = {
			{"block", RegistryIds::getBlock()},
			{"item", RegistryIds::getItem()},
			{"entity_type", RegistryIds::getEntityType()},
			{"sound_event", RegistryIds::getSoundEvent()},
			{"particle_type", RegistryIds::getParticleType()},
			{"mob_effect", RegistryIds::getMobEffect()},
			{"block_entity_type", RegistryIds::getBlockEntityType()},
			{"menu", RegistryIds::getMenu()},
			{"recipe_type", RegistryIds::getRecipeType()},
			{"recipe_serializer", RegistryIds::getRecipeSerializer()}
		};

		for (const auto& [name, data] : sources) {
			if (!data.empty()) {
				registries.push_back(createRegistryFromMap(name, data, true));
			} else if (g_logger) {
				g_logger->logNetwork(LogLevel::Warn, "Empty registry data source for: " + name, "RegistryData");
			}
		}

		if (g_logger) {
			g_logger->logNetwork(LogLevel::Info, "Created " + std::to_string(registries.size()) +
			                     " essential registries using fallback", "RegistryData");
		}

	} catch (const std::exception& e) {
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Error, "Failed to create essential registries: " + std::string(e.what()), "RegistryData");
		}
		registries.clear();
	}

	return registries;
}

// ============================================================
// ==========         PACKET SENDING              ==========
// ============================================================

void sendRegistryDataBatch(Packet& packet, Server& server, const std::vector<RegistryData>& registries) {
	if (registries.empty()) {
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Warn, "No registries to send", "Configuration");
		}
		packet.setReturnPacket(PACKET_OK);
		return;
	}

	if (g_logger) {
		g_logger->logNetwork(LogLevel::Info, "Sending registry batch: " + std::to_string(registries.size()) + " registries", "Configuration");
	}

	int successCount = 0;
	int errorCount = 0;

	for (const auto& registry : registries) {
		if (!validateRegistryData(registry)) {
			if (g_logger) {
				g_logger->logNetwork(LogLevel::Error, "Invalid registry data: " + registry.getRegistryId(), "Configuration");
			}
			errorCount++;
			continue;
		}

		try {
			Buffer buff;
			std::vector<uint8_t> registryBytes = registry.serialize();
			buff.writeBytes(registryBytes);

			packet.sendPacket(PacketId::Configuration::Clientbound::REGISTRY_DATA, buff, server, false);
			successCount++;

			if (g_logger) {
				g_logger->logNetwork(LogLevel::Info, "Sent registry: " + registry.getRegistryId() +
				                     " (" + std::to_string(registry.getEntryCount()) + " entries)", "Configuration");
			}

		} catch (const std::exception& e) {
			errorCount++;
			if (g_logger) {
				g_logger->logNetwork(LogLevel::Error, "Failed to send registry " + registry.getRegistryId() +
				                     ": " + std::string(e.what()), "Configuration");
			}
		}
	}

	if (g_logger) {
		g_logger->logNetwork(LogLevel::Info, "Registry batch complete: " + std::to_string(successCount) +
		                     " sent, " + std::to_string(errorCount) + " errors", "Configuration");
	}

	packet.setReturnPacket(errorCount == 0 ? PACKET_OK : PACKET_ERROR);
}

void sendRegistryData(Packet& packet, Server& server) {
	if (g_logger) {
		g_logger->logNetwork(LogLevel::Info, "=== Sending Registry Data (0x07) ===", "Configuration");
	}

	try {
		std::vector<RegistryData> registries = parseMinecraftRegistries();

		if (registries.empty()) {
			if (g_logger) {
				g_logger->logNetwork(LogLevel::Error, "No registries parsed from minecraft_registries.h", "Configuration");
			}
			packet.setReturnPacket(PACKET_ERROR);
			return;
		}

		if (g_logger) {
			g_logger->logNetwork(LogLevel::Info, "Parsed " + std::to_string(registries.size()) + " registries", "Configuration");
		}

		sendRegistryDataBatch(packet, server, registries);

	} catch (const std::exception& e) {
		if (g_logger) {
			g_logger->logNetwork(LogLevel::Error, "Failed to send registry data: " + std::string(e.what()), "Configuration");
		}
		packet.setReturnPacket(PACKET_ERROR);
	}
}
