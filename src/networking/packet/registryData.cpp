#include "data/RegistryIds.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

/**
 * Sends Registry Data packet (0x07) to client during configuration phase.
 *
 * This implementation sends only essential registries that are commonly required
 * by Minecraft clients. Sending too many registries can cause connection issues.
 *
 * Packet structure (Configuration phase, ID 0x07):
 * - Registry ID (Identifier): The registry identifier (e.g., "minecraft:block")
 * - Entries (Array):
 *   - Length (VarInt): Number of entries
 *   - For each entry:
 *     - Entry ID (Identifier): Entry name (e.g., "minecraft:stone")
 *     - Has Data (Boolean): Whether this entry has NBT data (always false)
 */
void sendRegistryData(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Sending essential Registry Data packets (0x07) ===", "Configuration");

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

		// Define essential registries only - these are the most commonly required
		struct RegistryInfo {
			std::string						registryName;
			std::map<std::string, uint32_t> data;
		};

		std::vector<RegistryInfo> essentialRegistries = {{"block", registryIds.getBlock()},
														 {"item", registryIds.getItem()},
														 {"entity_type", registryIds.getEntityType()},
														 {"sound_event", registryIds.getSoundEvent()},
														 {"particle_type", registryIds.getParticleType()},
														 {"mob_effect", registryIds.getMobEffect()},
														 {"block_entity_type", registryIds.getBlockEntityType()},
														 {"menu", registryIds.getMenu()},
														 {"recipe_type", registryIds.getRecipeType()},
														 {"recipe_serializer", registryIds.getRecipeSerializer()}};

		// Send each registry as a separate packet
		for (const auto& registry : essentialRegistries) {
			// Skip empty registries
			if (registry.data.empty()) {
				g_logger->logNetwork(WARN, "Skipping empty registry: " + registry.registryName, "Configuration");
				continue;
			}

			// Create payload buffer for this registry
			Buffer payloadBuf;

			// Write registry identifier (namespace:path format)
			payloadBuf.writeIdentifier("minecraft:" + registry.registryName);

			// Write the number of entries in this registry
			payloadBuf.writeVarInt(static_cast<int>(registry.data.size()));

			// Write each entry in the registry
			for (const auto& entry : registry.data) {
				// Write entry identifier
				payloadBuf.writeIdentifier(entry.first);

				// Write whether the entry has data (always false for basic registries)
				payloadBuf.writeBool(false);
			}

			// Create the final packet with proper structure
			Buffer finalBuf;
			int	   packetId = 0x07; // Registry Data packet ID for configuration mode

			// Calculate total size including packet ID
			int packetIdSize	 = packet.getVarintSize(packetId);
			int totalPayloadSize = packetIdSize + payloadBuf.getData().size();

			// Write packet length
			finalBuf.writeVarInt(totalPayloadSize);
			// Write packet ID
			finalBuf.writeVarInt(packetId);
			// Write payload
			finalBuf.writeBytes(payloadBuf.getData());

			// Create new packet for this registry
			Packet* registryPacket	  = new Packet(packet);
			registryPacket->getData() = finalBuf;
			registryPacket->setPacketSize(finalBuf.getData().size());
			registryPacket->setReturnPacket(PACKET_SEND);

			// Queue the packet
			outgoingPackets->push(registryPacket);

			g_logger->logNetwork(INFO,
								 "Registry Data packet created for '" + registry.registryName + "' with " + std::to_string(registry.data.size()) +
										 " entries, size: " + std::to_string(finalBuf.getData().size()) + " bytes",
								 "Configuration");
		}

		g_logger->logNetwork(
				INFO, "All " + std::to_string(essentialRegistries.size()) + " essential Registry Data packets queued successfully", "Configuration");

		// Set return packet to OK since we've queued all the registry packets
		packet.setReturnPacket(PACKET_OK);

	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error creating Registry Data packets: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
	}
}
