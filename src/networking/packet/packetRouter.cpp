#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

void packetRouter(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();

	if (player == nullptr)
		return;

	switch (player->getPlayerState()) {
	case PlayerState::Handshake:
		handleHandshakePacket(packet, server);
		break;
	case PlayerState::Status:
		if (packet.getId() == 0x00) {
			handleStatusPacket(packet, server);
		} else if (packet.getId() == 0x01) {
			handlePingPacket(packet, server);
		} else {
			packet.getPlayer()->setPlayerState(PlayerState::None);
			packet.setReturnPacket(PACKET_DISCONNECT);
		}
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
