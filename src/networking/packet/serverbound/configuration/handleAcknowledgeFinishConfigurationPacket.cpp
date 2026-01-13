#include "network/packet.hpp"
#include "player.hpp"

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
