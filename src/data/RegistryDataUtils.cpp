#include "RegistryData.hpp"
#include "RegistryDataUtils.hpp"
#include "RegistryIds.hpp"
#include "logger.hpp"
#include "minecraftRegistries.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<RegistryData> parseMinecraftRegistries() {
	std::vector<RegistryData> registries;

	try {
		registries.reserve(REGISTRIES.size());

		for (const auto& registryPair : REGISTRIES) {
			const std::string& registryName = registryPair.first;
			const Registry&	   registry		= registryPair.second;

			if (registry.entries.empty()) {
				if (g_logger) {
					g_logger->logNetwork(WARN, "Skipping empty registry: " + registryName, "RegistryData");
				}
				continue;
			}

			RegistryData registryData(registryName);

			registryData.reserve(registry.entries.size());

			for (const auto& entry : registry.entries) {
				registryData.addEntry(entry.name, false);
			}

			if (validateRegistryData(registryData)) {
				registries.push_back(std::move(registryData));

				if (g_logger) {
					g_logger->logNetwork(INFO,
										 "Parsed registry: " + registryName + " with " + std::to_string(registry.entries.size()) + " entries",
										 "RegistryData");
				}
			} else {
				if (g_logger) {
					g_logger->logNetwork(ERROR, "Failed to validate parsed registry: " + registryName, "RegistryData");
				}
			}
		}

		if (g_logger) {
			g_logger->logNetwork(
					INFO, "Successfully parsed " + std::to_string(registries.size()) + " registries from minecraft_registries.h", "RegistryData");
		}

	} catch (const std::exception& e) {
		if (g_logger) {
			g_logger->logNetwork(ERROR, "Failed to parse minecraft registries: " + std::string(e.what()), "RegistryData");
		}
		registries.clear();
	}

	return registries;
}

void sendRegistryData(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Sending Registry Data packets (0x07) using parseMinecraftRegistries ===", "Configuration");

	try {
		std::vector<RegistryData> registries = parseMinecraftRegistries();

		if (registries.empty()) {
			g_logger->logNetwork(ERROR, "No registries parsed from minecraft_registries.h", "Configuration");
			packet.setReturnPacket(PACKET_ERROR);
			return;
		}

		g_logger->logNetwork(
				INFO, "Successfully parsed " + std::to_string(registries.size()) + " registries from minecraft_registries.h", "Configuration");

		sendRegistryDataBatch(packet, server, registries);

	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Failed to send registry data: " + std::string(e.what()), "Configuration");
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
		return false;
	}

	std::set<std::string> entryIds;
	for (const auto& entry : registry.getEntries()) {
		if (entry.entry_id.empty()) {
			if (g_logger) {
				g_logger->logNetwork(
						ERROR, "Registry validation failed: entry with empty ID in registry: " + registry.getRegistryId(), "RegistryData");
			}
			return false;
		}

		if (entryIds.find(entry.entry_id) != entryIds.end()) {
			if (g_logger) {
				g_logger->logNetwork(ERROR,
									 "Registry validation failed: duplicate entry ID '" + entry.entry_id +
											 "' in registry: " + registry.getRegistryId(),
									 "RegistryData");
			}
			return false;
		}

		entryIds.insert(entry.entry_id);
	}

	return true;
}

RegistryData createRegistryFromMap(const std::string& registryName, const std::map<std::string, uint32_t>& registryData, bool addNamespace) {
	std::string	 fullRegistryName = addNamespace ? "minecraft:" + registryName : registryName;
	RegistryData registry(fullRegistryName);

	registry.reserve(registryData.size());

	for (const auto& entry : registryData) {
		registry.addEntry(entry.first, false);
	}

	return registry;
}

std::vector<uint8_t> serializeRegistryPacket(const RegistryData& registry) {
	try {
		std::vector<uint8_t> registryBytes = registry.serialize();

		Buffer packetBuffer;

		int packetId		= RegistryData::PACKET_ID;
		int packetIdSize	= 1;
		int totalPacketSize = packetIdSize + static_cast<int>(registryBytes.size());

		packetBuffer.writeVarInt(totalPacketSize);

		packetBuffer.writeVarInt(packetId);

		packetBuffer.writeBytes(registryBytes);

		return packetBuffer.getData();

	} catch (const std::exception& e) {
		throw std::runtime_error("Failed to serialize registry packet: " + std::string(e.what()));
	}
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
			g_logger->logNetwork(ERROR, "No player found in packet for registry data batch", "Configuration");
		}
		packet.setReturnPacket(PACKET_ERROR);
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

	if (g_logger) {
		g_logger->logNetwork(INFO, "Sending registry data batch with " + std::to_string(registries.size()) + " registries", "Configuration");
	}

	int successCount = 0;
	int errorCount	 = 0;

	for (const auto& registry : registries) {
		if (!validateRegistryData(registry)) {
			if (g_logger) {
				g_logger->logNetwork(ERROR, "Invalid registry data for: " + registry.getRegistryId(), "Configuration");
			}
			errorCount++;
			continue;
		}

		try {
			std::vector<uint8_t> packetData = serializeRegistryPacket(registry);

			Packet* registryPacket = new Packet(packet);

			Buffer buffer;
			buffer.writeBytes(packetData);

			registryPacket->getData() = buffer;
			registryPacket->setPacketSize(packetData.size());
			registryPacket->setReturnPacket(PACKET_SEND);

			outgoingPackets->push(registryPacket);
			successCount++;

			if (g_logger) {
				g_logger->logNetwork(INFO,
									 "Queued registry: " + registry.getRegistryId() + " (" + std::to_string(registry.getEntryCount()) + " entries)",
									 "Configuration");
			}

		} catch (const std::exception& e) {
			errorCount++;
			if (g_logger) {
				g_logger->logNetwork(
						ERROR, "Exception while queuing registry " + registry.getRegistryId() + ": " + std::string(e.what()), "Configuration");
			}
		}
	}

	if (g_logger) {
		g_logger->logNetwork(INFO,
							 "Registry data batch complete: " + std::to_string(successCount) + " sent, " + std::to_string(errorCount) + " errors",
							 "Configuration");
	}

	packet.setReturnPacket((errorCount == 0) ? PACKET_OK : PACKET_ERROR);
}

std::vector<RegistryData> createAllEssentialRegistries() {
	std::vector<RegistryData> registries;

	try {
		registries = parseMinecraftRegistries();

		if (!registries.empty()) {
			if (g_logger) {
				g_logger->logNetwork(INFO, "Using parsed registries from minecraft_registries.h", "RegistryData");
			}
			return registries;
		}

		if (g_logger) {
			g_logger->logNetwork(WARN, "parseMinecraftRegistries returned empty, using fallback method", "RegistryData");
		}

		std::vector<std::pair<std::string, std::map<std::string, uint32_t>>> registrySources = {
				{"block", RegistryIds::getBlock()},
				{"item", RegistryIds::getItem()},
				{"entity_type", RegistryIds::getEntityType()},
				{"sound_event", RegistryIds::getSoundEvent()},
				{"particle_type", RegistryIds::getParticleType()},
				{"mob_effect", RegistryIds::getMobEffect()},
				{"block_entity_type", RegistryIds::getBlockEntityType()},
				{"menu", RegistryIds::getMenu()},
				{"recipe_type", RegistryIds::getRecipeType()},
				{"recipe_serializer", RegistryIds::getRecipeSerializer()}};

		for (const auto& source : registrySources) {
			if (!source.second.empty()) {
				RegistryData registry = createRegistryFromMap(source.first, source.second, true);
				registries.push_back(std::move(registry));
			} else {
				if (g_logger) {
					g_logger->logNetwork(WARN, "Registry data source is empty for: " + source.first, "RegistryData");
				}
			}
		}

		if (g_logger) {
			g_logger->logNetwork(
					INFO, "Created " + std::to_string(registries.size()) + " essential registries using fallback method", "RegistryData");
		}

	} catch (const std::exception& e) {
		if (g_logger) {
			g_logger->logNetwork(ERROR, "Failed to create essential registries: " + std::string(e.what()), "RegistryData");
		}
		registries.clear();
	}

	return registries;
}
