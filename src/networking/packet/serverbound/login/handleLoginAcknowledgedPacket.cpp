#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void handleLoginAcknowledgedPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}
	player->setPlayerState(PlayerState::Configuration);
	packet.setReturnPacket(PACKET_OK);
	(void)server;
}
