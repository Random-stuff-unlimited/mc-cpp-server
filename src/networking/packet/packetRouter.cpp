#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

void initConnectionSequence(Packet* packet,
                            ThreadSafeQueue<Packet*>* _outgoingPackets,
                            Server& server) {
	Packet* login = new Packet(*packet);
	_outgoingPackets->push(login);

	Packet* playPacket = new Packet(*packet);
	writePlaytPacket(*playPacket, server);
	_outgoingPackets->push(playPacket);

	packet->setReturnPacket(PACKET_OK);
}

void packetRouter(Packet* packet, Server& server, ThreadSafeQueue<Packet*>* _outgoingPackets) {
	if (packet == nullptr) return;

	Player* player = packet->getPlayer();

	if (player == nullptr) return;

	// Vous pouvez maintenant utiliser incomingPackets dans cette fonction
	// Par exemple: incomingPackets->push(nouveauPacket);

	switch (player->getPlayerState()) {
	case PlayerState::Handshake:
		handleHandshakePacket(*packet, server);
		break;
	case PlayerState::Status:
		if (packet->getId() == 0x00) {
			handleStatusPacket(*packet, server);
		} else if (packet->getId() == 0x01) {
			handlePingPacket(*packet, server);
		} else {
			packet->getPlayer()->setPlayerState(PlayerState::None);
			packet->setReturnPacket(PACKET_DISCONNECT);
		}
		break;
	case PlayerState::Login:
		handleLoginStartPacket(*packet, server);
		initConnectionSequence(packet, _outgoingPackets, server);
		break;
	case PlayerState::Play:
		// handlePlayPacket(*packet, server);
		break;
	case PlayerState::Configuration:
	        if (packet.getId() == 0x00) {
                handleClientInformation(packet);
	        } else {
	            packet.getPlayer()->setPlayerState(PlayerState::None);
	            packet.setReturnPacket(PACKET_DISCONNECT);
	        }
	    break;
	default:
		// État inconnu
		break;
	}
}
