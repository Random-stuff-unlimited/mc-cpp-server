#include "logger.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

// The configuration packets the server receives. One file per state, by direction.

// ServerboundKnownPacksPacket: the packs the client has, so the server knows what its Registry Data refers to
void serverboundKnownPacksPacket(Packet& packet) {
	packet.getData().readVarInt(); // Number of packs
}

// ServerboundClientInformationPacket: the client's settings (locale, view distance, skin parts, hands...)
void handleClientInformationPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		g_logger->logNetwork(ERROR, "No player associated with Client Information packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	PlayerConfig* config = player->getPlayerConfig();
	if (!config) {
		g_logger->logNetwork(ERROR, "Player config is null for Client Information packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	try {
		config->setLocale(packet.getData().readString(16));		 // Locale, e.g. "en_US"
		config->setViewDistance(packet.getData().readByte());	 // View distance
		config->setChatMode(packet.getData().readVarInt());		 // Chat mode
		config->setChatColors(packet.getData().readByte() != 0); // Chat colors
		config->setDisplayedSkinParts(packet.getData().readByte());
		config->setMainHand(packet.getData().readVarInt());
		config->setTextFiltering(packet.getData().readByte() != 0);
		config->setServerListings(packet.getData().readByte() != 0);
		packet.setReturnPacket(PACKET_OK);
	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error reading Client Information packet: " + std::string(e.what()), "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
	}

	(void)server;
}

// ServerboundFinishConfigurationPacket: the client acknowledged the end of the configuration, it is in Play now
void handleAcknowledgeFinishConfigurationPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	player->setPlayerState(PlayerState::Play);
	packet.setReturnPacket(PACKET_OK);

	(void)server;
}