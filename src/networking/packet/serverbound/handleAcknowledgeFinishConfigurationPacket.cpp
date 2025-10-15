#include "network/packet.hpp"
#include "player.hpp"

void handleAcknowledgeFinishConfigurationPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	// Client has acknowledged finish configuration, now transition to Play state
	player->setPlayerState(PlayerState::Play);

	// g_logger->logNetwork(INFO, "Player " + player->getPlayerName() + " transitioned to Play state
	// - ready for game packets", "Configuration");

	// Just acknowledge the packet - the actual game sequence will be triggered separately
	packet.setReturnPacket(PACKET_OK);

	(void)server; // Suppress unused parameter warning
}
