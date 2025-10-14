#include "data/RegistriesTag.hpp"
#include "data/TagUtils.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void sendUpdateTags(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Sending Update Tags packet (0x0D) ===", "Configuration");

	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(ERROR, "No player associated with Update Tags packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	ThreadSafeQueue<Packet*>* outgoingPackets = server.getNetworkManager().getOutgoingQueue();
	if (!outgoingPackets) {
		g_logger->logNetwork(ERROR, "No outgoing packet queue available", "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
		return;
	}

	try {
		TagUtils::logTagStatistics();

		Buffer tagBuffer;
		size_t totalRegistries = RegistriesTags.size();
		size_t totalTags	   = TagUtils::getTotalTagCount();
		size_t totalEntries	   = 0;

		tagBuffer.writeVarInt(static_cast<int32_t>(totalRegistries));

		for (const auto& [registryName, tags] : RegistriesTags) {
			tagBuffer.writeString(registryName);
			tagBuffer.writeVarInt(static_cast<int32_t>(tags.size()));

			size_t registryEntries = 0;
			for (const auto& tag : tags) {
				tagBuffer.writeString(tag.name);
				tagBuffer.writeVarInt(static_cast<int32_t>(tag.entries.size()));

				for (int entryId : tag.entries) {
					tagBuffer.writeVarInt(entryId);
				}
				registryEntries += tag.entries.size();
			}

			totalEntries += registryEntries;
			g_logger->logNetwork(DEBUG,
								 "Registry: " + registryName + " -> " + std::to_string(tags.size()) + " tags, " + std::to_string(registryEntries) +
										 " entries",
								 "Configuration");
		}

		Buffer finalBuf;
		int	   packetId			= 0x0D;
		int	   packetIdSize		= packet.getVarintSize(packetId);
		int	   totalPayloadSize = packetIdSize + tagBuffer.getData().size();

		finalBuf.writeVarInt(totalPayloadSize);
		finalBuf.writeVarInt(packetId);
		finalBuf.writeBytes(tagBuffer.getData());

		Packet* tagsPacket	  = new Packet(packet);
		tagsPacket->getData() = finalBuf;
		tagsPacket->setPacketSize(finalBuf.getData().size());
		tagsPacket->setReturnPacket(PACKET_SEND);

		outgoingPackets->push(tagsPacket);

		g_logger->logNetwork(INFO,
							 "Update Tags packet sent: " + std::to_string(totalRegistries) + " registries, " + std::to_string(totalTags) + " tags, " +
									 std::to_string(totalEntries) + " entries, packet size: " + std::to_string(finalBuf.getData().size()) + " bytes",
							 "Configuration");

		packet.setReturnPacket(PACKET_OK);

	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Failed to send Update Tags packet: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
	}
}
