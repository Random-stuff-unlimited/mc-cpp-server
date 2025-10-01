#include "packet.hpp"
#include "server.hpp"
#include "enums.hpp"
#include "networking.hpp"

void packetRouter(Packet &packet, Server &server) {
	Player  *player = packet.getPlayer();

	if(player == NULL)
		return;

	switch(player->getPlayerState()) {
		case PlayerState::Handshake:
			handleHandshakePacket(packet, server);
			break;
		case PlayerState::Status:
			handleStatusPacket(packet, server);
			break;
		case PlayerState::Login:
			// handleLoginPacket(packet, server);
			break;
		case PlayerState::Play:
			// handlePlayPacket(packet, server);
			break;
		default:
			// État inconnu
			break;
	}
}