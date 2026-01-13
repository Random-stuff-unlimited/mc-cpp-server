#include "data/RegistriesTag.hpp"
#include "data/TagUtils.hpp"
#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void sendUpdateTags(Packet& packet, Server& server) {
	g_logger->logNetwork(LogLevel::Info, "=== Sending Update Tags packet (0x0D) ===", "Configuration");

	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(LogLevel::Error, "No player associated with Update Tags packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	ThreadSafeQueue<Packet*>* outgoingPackets = server.getNetworkManager().getOutgoingQueue();
	if (!outgoingPackets) {
		g_logger->logNetwork(LogLevel::Error, "No outgoing packet queue available", "Configuration");
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
			g_logger->logNetwork(LogLevel::Debug,
								 "Registry: " + registryName + " -> " + std::to_string(tags.size()) + " tags, " + std::to_string(registryEntries) +
										 " entries",
								 "Configuration");
		}

		Packet* tagsPacket = new Packet(packet);
		tagsPacket->sendPacket(0x0D, tagBuffer, server, true);

		g_logger->logNetwork(LogLevel::Info,
							 "Update Tags packet sent: " + std::to_string(totalRegistries) + " registries, " + std::to_string(totalTags) + " tags, " +
									 std::to_string(totalEntries) + " entries",
							 "Configuration");

		packet.setReturnPacket(PACKET_OK);

	} catch (const std::exception& e) {
		g_logger->logNetwork(LogLevel::Error, "Failed to send Update Tags packet: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_ERROR);
	}
}
