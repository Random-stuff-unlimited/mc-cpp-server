#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include <iostream>

void initConnectionSequence(Packet* packet,
                            ThreadSafeQueue<Packet*>* _outgoingPackets,
                            Server& server) {
	if (packet == nullptr || _outgoingPackets == nullptr)
		return;

	Player* player = packet->getPlayer();
	if (player == nullptr) return;

	player->setPlayerState(PlayerState::Play);

	try {
		// 1. Send Login (play) packet - 0x2B
		Packet* playPacket = new Packet(*packet);
		writePlaytPacket(*playPacket, server);
		_outgoingPackets->push(playPacket);

		// 2. Send Set Center Chunk - 0x57
		Packet* setCenterPacket = new Packet(*packet);
		writeSetCenterPacket(*setCenterPacket, server);
		_outgoingPackets->push(setCenterPacket);

		// 3. Send complete chunk batch sequence (Start -> Chunks -> Finished)
		sendChunkBatchSequence(*packet, server, _outgoingPackets);

		// 4. Send player position and look - 0x41
		Packet* positionPacket = new Packet(*packet);
		sendPlayerPositionAndLook(*positionPacket, server);
		_outgoingPackets->push(positionPacket);

		// 5. Send spawn position - 0x5A
		Packet* spawnPacket = new Packet(*packet);
		sendSpawnPosition(*spawnPacket, server);
		_outgoingPackets->push(spawnPacket);

		// 6. Complete spawn sequence (abilities, health, experience, time, held item)
		completeSpawnSequence(*packet, server, _outgoingPackets);

		std::cout << "=== Complete connection sequence sent to player: "
		          << player->getPlayerName() << " ===\n";

		packet->setReturnPacket(PACKET_OK);
	} catch (const std::exception& e) {
		std::cerr << "Error in connection sequence: " << e.what() << std::endl;
		packet->setReturnPacket(PACKET_ERROR);
	}
}

void packetRouter(Packet* packet, Server& server, ThreadSafeQueue<Packet*>* _outgoingPackets) {
	if (packet == nullptr) return;
	if (_outgoingPackets == nullptr) return;

	Player* player = packet->getPlayer();

	if (player == nullptr) {
		packet->setReturnPacket(PACKET_DISCONNECT);
		return;
	}

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
		if (packet->getId() == 0x00) {
			handleLoginStartPacket(*packet, server);
		} else if (packet->getId() == 0x03) {
			handleLoginAcknowledged(*packet, server);
		} else {
			player->setPlayerState(PlayerState::None);
			packet->setReturnPacket(PACKET_DISCONNECT);
		}
		break;
	case PlayerState::Play:
		if (packet->getId() == 0x00) {
			handleConfirmTeleportation(*packet, server);
		} else if (packet->getId() == 0x1B) {
			//keep_alive
		} else {
			// Other play packets would go here
		}
		break;
	case PlayerState::Configuration:
		if (packet->getId() == 0x00) {
			initConnectionSequence(packet, _outgoingPackets, server);
		} else if (packet->getId() == 0x03) {
			handleClientInformation(*packet, server);
		} else {
			player->setPlayerState(PlayerState::None);
			packet->setReturnPacket(PACKET_DISCONNECT);
		}
		break;
	default:
		// État inconnu
		break;
	}
}
