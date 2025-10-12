#include "RegistryData.hpp"
#include "RegistryIds.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

/**
 * Sends Registry Data packet (0x07) to client during configuration phase.
 *
 * This implementation uses the new RegistryData class for better structure
 * and error handling. It sends only essential registries that are commonly
 * required by Minecraft clients.
 *
 * Packet structure (Configuration phase, ID 0x07):
 * - Registry ID (Identifier): The registry identifier (e.g., "minecraft:block")
 * - Entries (Array):
 *   - Length (VarInt): Number of entries
 *   - For each entry:
 *     - Entry ID (Identifier): Entry name (e.g., "minecraft:stone")
 *     - Has Data (Boolean): Whether this entry has NBT data (always false for basic registries)
 *     - Data (Optional NBT): NBT data if has_data is true
 */
void sendRegistryData(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Sending Registry Data packets (0x07) using new RegistryData class ===", "Configuration");

	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(ERROR, "Error: No player associated with Registry Data packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	ThreadSafeQueue<Packet*>* outgoingPackets = server.getNetworkManager().getOutgoingQueue();
	if (!outgoingPackets) {
		g_logger->logNetwork(ERROR, "Error: No outgoing packet queue available", "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
		return;
	}

	try {
		RegistryIds registryIds;

		// Define essential registries with their data sources
		struct RegistryInfo {
			std::string						registryName;
			std::map<std::string, uint32_t> data;
		};

		std::vector<RegistryInfo> essentialRegistries = {
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

		int successfulRegistries = 0;
		int totalPacketsSent = 0;

		// Process each registry
		for (const auto& registryInfo : essentialRegistries) {
			// Skip empty registries
			if (registryInfo.data.empty()) {
				g_logger->logNetwork(WARN, "Skipping empty registry: " + registryInfo.registryName, "Configuration");
				continue;
			}

			try {
				// Create RegistryData object
				RegistryData registryData("minecraft:" + registryInfo.registryName);

				// Reserve space for efficiency
				registryData.reserve(registryInfo.data.size());

				// Add all entries to the registry (basic registries don't have NBT data)
				for (const auto& entry : registryInfo.data) {
					registryData.addEntry(entry.first, false); // has_data = false for basic registries
				}

				// Serialize the registry data
				std::vector<uint8_t> serializedData = registryData.serialize();

				// Create the final packet with proper structure
				Buffer finalBuf;
				int	   packetId = RegistryData::PACKET_ID; // 0x07

				// Calculate total size including packet ID
				int packetIdSize	 = packet.getVarintSize(packetId);
				int totalPayloadSize = packetIdSize + serializedData.size();

				// Write packet length
				finalBuf.writeVarInt(totalPayloadSize);
				// Write packet ID
				finalBuf.writeVarInt(packetId);
				// Write serialized registry data
				finalBuf.writeBytes(serializedData);

				// Create new packet for this registry
				Packet* registryPacket	  = new Packet(packet);
				registryPacket->getData() = finalBuf;
				registryPacket->setPacketSize(finalBuf.getData().size());
				registryPacket->setReturnPacket(PACKET_SEND);

				// Queue the packet
				outgoingPackets->push(registryPacket);

				successfulRegistries++;
				totalPacketsSent++;

				g_logger->logNetwork(INFO,
									 "Registry Data packet created for '" + registryInfo.registryName + "' with " +
									 std::to_string(registryData.getEntryCount()) + " entries, size: " +
									 std::to_string(finalBuf.getData().size()) + " bytes",
									 "Configuration");

				// Log debug information about the registry
				g_logger->logNetwork(DEBUG, "Registry details: " + registryData.toString(), "Configuration");

			} catch (const std::exception& e) {
				g_logger->logNetwork(ERROR,
									 "Failed to create registry packet for '" + registryInfo.registryName +
									 "': " + std::string(e.what()),
									 "Configuration");
				// Continue with other registries instead of failing completely
				continue;
			}
		}

		// Log summary
		g_logger->logNetwork(INFO,
							 "Registry Data processing complete: " + std::to_string(successfulRegistries) +
							 "/" + std::to_string(essentialRegistries.size()) + " registries processed, " +
							 std::to_string(totalPacketsSent) + " packets queued",
							 "Configuration");

		if (successfulRegistries == 0) {
			g_logger->logNetwork(ERROR, "No registry data packets were successfully created", "Configuration");
			packet.setReturnPacket(PACKET_ERROR);
		} else {
			// Set return packet to OK since we've queued at least some registry packets
			packet.setReturnPacket(PACKET_OK);
		}

	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Critical error in Registry Data processing: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
	}
}

/**
 * Alternative function to create a single RegistryData object with all registries
 * This can be used if you prefer to send all registries in one packet instead of separate packets
 */
RegistryData createCombinedRegistryData() {
	RegistryData combinedRegistry("minecraft:combined");
	RegistryIds registryIds;

	try {
		// Add entries from all essential registries
		std::vector<std::pair<std::string, std::map<std::string, uint32_t>>> registries = {
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

		size_t totalEntries = 0;
		for (const auto& reg : registries) {
			totalEntries += reg.second.size();
		}

		combinedRegistry.reserve(totalEntries);

		for (const auto& reg : registries) {
			for (const auto& entry : reg.second) {
				// Prefix entry IDs with registry type for disambiguation
				std::string prefixedId = reg.first + ":" + entry.first;
				combinedRegistry.addEntry(prefixedId, false);
			}
		}

		if (g_logger) {
			g_logger->logNetwork(INFO,
								 "Created combined registry with " + std::to_string(combinedRegistry.getEntryCount()) +
								 " total entries",
								 "Configuration");
		}

	} catch (const std::exception& e) {
		if (g_logger) {
			g_logger->logNetwork(ERROR, "Failed to create combined registry: " + std::string(e.what()), "Configuration");
		}
	}

	return combinedRegistry;
}
