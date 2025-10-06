#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include "logger.hpp"
#include <iostream>

void initGameSequence(Packet* packet,
                      ThreadSafeQueue<Packet*>* _outgoingPackets,
                      Server& server) {
	if (packet == nullptr || _outgoingPackets == nullptr)
		return;

	Player* player = packet->getPlayer();
	if (player == nullptr) return;

	// Player should already be in Play state at this point
	g_logger->logNetwork(INFO, "Starting game sequence for player: " + player->getPlayerName(), "Play");

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

		g_logger->logNetwork(INFO, "Complete game sequence sent to player: " + player->getPlayerName(), "Play");

		packet->setReturnPacket(PACKET_OK);
	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error in game sequence: " + std::string(e.what()), "Play");
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
		} else if (packet->getId() == 0x02) {
			// Login Plugin Response
			g_logger->logNetwork(INFO, "Received Login Plugin Response", "Login");
			packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x03) {
			handleLoginAcknowledged(*packet, server);
		} else if (packet->getId() == 0x04) {
			// Cookie Response (login)
			g_logger->logNetwork(INFO, "Received Login Cookie Response", "Login");
			packet->setReturnPacket(PACKET_OK);
		} else {
			player->setPlayerState(PlayerState::None);
			packet->setReturnPacket(PACKET_DISCONNECT);
		}
		break;
	case PlayerState::Configuration:
		if (packet->getId() == 0x00) {
			// Client Information (configuration)
			g_logger->logNetwork(INFO, "Received Client Information in Configuration state", "Configuration");
			handleClientInformation(*packet, server);
			packet->setReturnPacket(PACKET_OK);

			// After processing client info, tell client we're done configuring
			Packet* finishPacket = new Packet(*packet);
			handleFinishConfiguration(*finishPacket, server);
			_outgoingPackets->push(finishPacket);
			g_logger->logNetwork(INFO, "Sent Finish Configuration after Client Information", "Configuration");

		} else if (packet->getId() == 0x01) {
			// Cookie Response (configuration)
			g_logger->logNetwork(INFO, "Received Cookie Response in Configuration state", "Configuration");
			packet->setReturnPacket(PACKET_OK);

		} else if (packet->getId() == 0x02) {
			// Serverbound Plugin Message (configuration)
			g_logger->logNetwork(INFO, "Received Serverbound Plugin Message in Configuration state (0x02), size: " + std::to_string(packet->getSize()) + " bytes", "Configuration");
			packet->setReturnPacket(PACKET_OK);

		} else if (packet->getId() == 0x03) {
			// Acknowledge Finish Configuration -> enter Play
			g_logger->logNetwork(INFO, "Received Acknowledge Finish Configuration - transitioning to Play state", "Configuration");
			handleAcknowledgeFinishConfiguration(*packet, server);
			initGameSequence(packet, _outgoingPackets, server);

		} else if (packet->getId() == 0x04) {
			// Keep Alive (configuration)
			g_logger->logNetwork(INFO, "Received Keep Alive in Configuration state", "Configuration");
			packet->setReturnPacket(PACKET_OK);

		} else if (packet->getId() == 0x05) {
			// Pong (configuration)
			g_logger->logNetwork(INFO, "Received Pong in Configuration state", "Configuration");
			packet->setReturnPacket(PACKET_OK);

		} else if (packet->getId() == 0x06) {
			// Resource Pack Response (configuration)
			g_logger->logNetwork(INFO, "Received Resource Pack Response in Configuration state", "Configuration");
			packet->setReturnPacket(PACKET_OK);

		} else if (packet->getId() == 0x07) {
			// Serverbound Known Packs (configuration)
			g_logger->logNetwork(INFO, "Received Serverbound Known Packs in Configuration state", "Configuration");
			packet->setReturnPacket(PACKET_OK);

		} else if (packet->getId() == 0x08) {
			// Custom Click Action (configuration)
			g_logger->logNetwork(INFO, "Received Custom Click Action in Configuration state", "Configuration");
			packet->setReturnPacket(PACKET_OK);

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
			g_logger->logNetwork(INFO, "Received packet ID: " + std::to_string(packet->getId()) + " in Play state", "Play");
		}
		break;
	default:
		// État inconnu
		break;
	}
}
