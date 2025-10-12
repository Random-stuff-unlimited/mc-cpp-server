#include "RegistryDataUtils.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

/**
 * Sends Update Tags packet (0x0D) to client during configuration phase.
 *
 * Packet structure (Configuration phase, ID 0x0D):
 * - Array of tags (Registry):
 *   - Registry identifier (Identifier): The registry identifier (e.g., "minecraft:block")
 *   - Array of Tag:
 *     - Tag name (Identifier): Tag identifier (e.g., "minecraft:planks")
 *     - Count (VarInt): Number of entries in this tag
 *     - Entries (Array of VarInt): Registry IDs of entries in this tag
 */
void sendUpdateTags(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Sending Update Tags packet (0x0D) ===", "Configuration");

	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(ERROR, "Error: No player associated with Update Tags packet", "Configuration");
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
		Buffer tagBuffer;

		// Define required registries for tags (Vanilla expects these)
		std::vector<std::string> requiredRegistries = {
				"minecraft:block", "minecraft:item", "minecraft:fluid", "minecraft:entity_type", "minecraft:game_event"};

		// Write number of registries
		tagBuffer.writeVarInt(requiredRegistries.size());

		for (const std::string& registryId : requiredRegistries) {
			// Write registry identifier
			tagBuffer.writeString(registryId);

			// For now, send empty tag arrays for each registry
			// In a full implementation, you would load actual tag data
			tagBuffer.writeVarInt(0); // Number of tags in this registry

			g_logger->logNetwork(DEBUG, "Added empty tag registry: " + registryId, "Configuration");
		}

		// Create the final packet
		Buffer finalBuf;
		int	   packetId = 0x0D; // Update Tags packet ID

		// Calculate total size including packet ID
		int packetIdSize	 = packet.getVarintSize(packetId);
		int totalPayloadSize = packetIdSize + tagBuffer.getData().size();

		// Write packet length
		finalBuf.writeVarInt(totalPayloadSize);
		// Write packet ID
		finalBuf.writeVarInt(packetId);
		// Write tag data
		finalBuf.writeBytes(tagBuffer.getData());

		// Create new packet for tags
		Packet* tagsPacket	  = new Packet(packet);
		tagsPacket->getData() = finalBuf;
		tagsPacket->setPacketSize(finalBuf.getData().size());
		tagsPacket->setReturnPacket(PACKET_SEND);

		// Queue the packet
		outgoingPackets->push(tagsPacket);

		g_logger->logNetwork(INFO,
							 "Update Tags packet created with " + std::to_string(requiredRegistries.size()) +
									 " registries, size: " + std::to_string(finalBuf.getData().size()) + " bytes",
							 "Configuration");

		packet.setReturnPacket(PACKET_OK);

	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Critical error in Update Tags processing: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
	}
}