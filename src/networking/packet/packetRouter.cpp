#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <string>

void handleHandshakeState(Packet* packet, Server& server);
void handleStatusState(Packet* packet, Server& server);
void handleLoginState(Packet* packet, Server& server);
void handleConfigurationState(Packet* packet, Server& server);
void handlePlayState(Packet* packet, Server& server);
void sendDisconnectPacket(Packet* packet, const std::string& reason, Server& server);
void initGameSequence(Packet* packet, Server& server);

// ========================================
// Main Packet Router
// ========================================

void packetRouter(Packet* packet, Server& server) {

	if (packet == nullptr) return;
	if (server.getNetworkManager().getOutgoingQueue() == nullptr) return;

	Player* player = packet->getPlayer();

	if (player == nullptr) {
		packet->setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	g_logger->logNetwork(INFO,
						 "Routing packet ID: 0x" + std::to_string(packet->getId()) + " (size: " + std::to_string(packet->getSize()) +
								 ") for state: " + std::to_string(static_cast<int>(player->getPlayerState())),
						 "PacketRouter");

	switch (player->getPlayerState()) {
	case PlayerState::Handshake:
		handleHandshakeState(packet, server);
		break;
	case PlayerState::Status:
		handleStatusState(packet, server);
		break;
	case PlayerState::Login:
		handleLoginState(packet, server);
		break;
	case PlayerState::Configuration:
		handleConfigurationState(packet, server);
		break;
	case PlayerState::Play:
		handlePlayState(packet, server);
		break;
	default:
		g_logger->logNetwork(
				WARN, "Unknown player state: " + std::to_string(static_cast<int>(player->getPlayerState())) + ", disconnecting", "PacketRouter");
		packet->setReturnPacket(PACKET_DISCONNECT);
		break;
	}
}

// ========================================
// Handshake State Handler
// ========================================

void handleHandshakeState(Packet* packet, Server& server) { handleHandshakePacket(*packet, server); }

// ========================================
// Status State Handler
// ========================================

void handleStatusState(Packet* packet, Server& server) {
	if (packet->getId() == 0x00) {
		handleStatusPacket(*packet, server);
	} else if (packet->getId() == 0x01) {
		handlePingPacket(*packet, server);
	} else {
		g_logger->logNetwork(WARN, "Unknown packet ID in Status state: 0x" + std::to_string(packet->getId()), "PacketRouter");
		packet->getPlayer()->setPlayerState(PlayerState::None);
		packet->setReturnPacket(PACKET_DISCONNECT);
	}
}

// ========================================
// Login State Handler
// ========================================

void handleLoginState(Packet* packet, Server& server) {
	// Add safety check for packet data integrity
	if (packet->getSize() > 32767) { // Max packet size
		g_logger->logNetwork(ERROR, "Packet size too large: " + std::to_string(packet->getSize()), "PacketRouter");
		packet->setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	if (packet->getId() == 0x00) {
		// Login Start
		g_logger->logNetwork(INFO, "Processing Login Start (0x00)", "PacketRouter");
		handleLoginStartPacket(*packet, server);
	} else if (packet->getId() == 0x02) {
		// Login Plugin Response - safe to ignore most of the time
		g_logger->logNetwork(INFO, "Received Login Plugin Response (0x02) - acknowledging", "PacketRouter");
		packet->setReturnPacket(PACKET_OK);
	} else if (packet->getId() == 0x03) {
		// Login Acknowledged
		g_logger->logNetwork(INFO, "Processing Login Acknowledged (0x03)", "PacketRouter");
		handleLoginAcknowledged(*packet, server);
	} else if (packet->getId() == 0x04) {
		// Cookie Response (login)
		g_logger->logNetwork(INFO, "Received Login Cookie Response (0x04) - acknowledging", "PacketRouter");
		packet->setReturnPacket(PACKET_OK);
	} else {
		g_logger->logNetwork(WARN, "Unknown packet ID in Login state: 0x" + std::to_string(packet->getId()), "PacketRouter");
		packet->getPlayer()->setPlayerState(PlayerState::None);
		packet->setReturnPacket(PACKET_DISCONNECT);
	}
}

// ========================================
// Configuration State Handler
// ========================================

void handleConfigurationState(Packet* packet, Server& server) {
	if (packet->getId() == 0x00) {
		// Client Information (configuration)
		// g_logger->logNetwork(INFO, "Received Client Information in Configuration state",
		// "Configuration");
		handleClientInformation(*packet, server);
		packet->setReturnPacket(PACKET_OK);

		// After processing client info, tell client we're done configuring
		Packet* finishPacket = new Packet(*packet);
		handleFinishConfiguration(*finishPacket, server);
		server.getNetworkManager().getOutgoingQueue()->push(finishPacket);
		// g_logger->logNetwork(INFO, "Sent Finish Configuration after Client Information",
		// "Configuration");

	} else if (packet->getId() == 0x01) {
		// Cookie Response (configuration)
		// g_logger->logNetwork(INFO, "Received Cookie Response in Configuration state",
		// "Configuration");
		packet->setReturnPacket(PACKET_OK);

	} else if (packet->getId() == 0x02) {
		// Serverbound Plugin Message (configuration)
		g_logger->logNetwork(
				INFO, "Received Serverbound Plugin Message (0x02), size: " + std::to_string(packet->getSize()) + " bytes", "PacketRouter");
		packet->setReturnPacket(PACKET_OK);

	} else if (packet->getId() == 0x03) {
		// Acknowledge Finish Configuration -> enter Play
		g_logger->logNetwork(INFO, "Received Acknowledge Finish Configuration - transitioning to Play state", "PacketRouter");
		handleAcknowledgeFinishConfiguration(*packet, server);
		initGameSequence(packet, server);

	} else if (packet->getId() == 0x04) {
		// Keep Alive (configuration)
		// g_logger->logNetwork(INFO, "Received Keep Alive in Configuration state",
		// "Configuration");
		packet->setReturnPacket(PACKET_OK);

	} else if (packet->getId() == 0x05) {
		// Pong (configuration)
		// g_logger->logNetwork(INFO, "Received Pong in Configuration state", "Configuration");
		packet->setReturnPacket(PACKET_OK);

	} else if (packet->getId() == 0x06) {
		// Resource Pack Response (configuration)
		// g_logger->logNetwork(INFO, "Received Resource Pack Response in Configuration state",
		// "Configuration");
		packet->setReturnPacket(PACKET_OK);

	} else if (packet->getId() == 0x07) {
		// Serverbound Known Packs (configuration)
		// g_logger->logNetwork(INFO, "Received Serverbound Known Packs in Configuration state",
		// "Configuration");
		packet->setReturnPacket(PACKET_OK);

	} else if (packet->getId() == 0x08) {
		// Custom Click Action (configuration)
		// g_logger->logNetwork(INFO, "Received Custom Click Action in Configuration state",
		// "Configuration");
		packet->setReturnPacket(PACKET_OK);

	} else {
		g_logger->logNetwork(WARN, "Unknown packet ID in Configuration state: 0x" + std::to_string(packet->getId()), "PacketRouter");
		sendDisconnectPacket(packet, "Unknown packet in Configuration state", server);
		packet->getPlayer()->setPlayerState(PlayerState::None);
		packet->setReturnPacket(PACKET_DISCONNECT);
	}
}

// ========================================
// Play State Handler
// ========================================

void handlePlayState(Packet* packet, Server& server) {
	if (packet->getId() == 0x00) {
		// Confirm Teleportation
		handleConfirmTeleportation(*packet, server);
	} else if (packet->getId() == 0x1B) {
		// Keep Alive
		packet->setReturnPacket(PACKET_OK);
	} else {
		// Other play packets
		// g_logger->logNetwork(INFO, "Received packet ID: " + std::to_string(packet->getId()) + "
		// in Play state", "Play");
		packet->setReturnPacket(PACKET_OK);
	}
}

// ========================================
// Disconnect Packet Creation
// ========================================

void sendDisconnectPacket(Packet* packet, const std::string& reason, Server& server) {
	ThreadSafeQueue<Packet*>* outgoingPackets = server.getNetworkManager().getOutgoingQueue();
	if (!packet || !outgoingPackets) return;

	Player* player = packet->getPlayer();
	if (!player) return;

	g_logger->logNetwork(INFO, "Sending disconnect packet to player with reason: " + reason, "PacketRouter");

	try {
		// Create Disconnect packet (0x02 in Configuration state)
		Buffer payload;
		payload.writeVarInt(0x02); // Disconnect packet ID

		// Create JSON reason
		std::string jsonReason = "{\"text\":\"" + reason + "\"}";
		payload.writeString(jsonReason);

		Buffer final;
		final.writeVarInt(payload.getData().size());
		final.writeBytes(payload.getData());

		// Create new packet for disconnect
		Packet* disconnectPacket	= new Packet(*packet);
		disconnectPacket->getData() = final;
		disconnectPacket->setReturnPacket(PACKET_SEND);
		disconnectPacket->setPacketSize(final.getData().size());

		outgoingPackets->push(disconnectPacket);

		g_logger->logNetwork(INFO, "Disconnect packet queued for sending", "PacketRouter");
	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error creating disconnect packet: " + std::string(e.what()), "PacketRouter");
	}
}

// ========================================
// Game Initialization Sequence
// ========================================

void initGameSequence(Packet* packet, Server& server) {
	if (packet == nullptr || server.getNetworkManager().getOutgoingQueue() == nullptr) return;

	Player* player = packet->getPlayer();
	if (player == nullptr) return;

	// Player should already be in Play state at this point
	g_logger->logNetwork(INFO, "Starting game sequence for player: " + player->getPlayerName(), "PacketRouter");

	try {
		// 1. Send Login (play) packet - 0x2B
		g_logger->logNetwork(INFO, "Sending Login (play) packet", "PacketRouter");
		Packet* playPacket = new Packet(*packet);
		writePlayPacket(*playPacket);
		server.getNetworkManager().getOutgoingQueue()->push(playPacket);

		// 2. Send Set Center Chunk - 0x57
		Packet* setCenterPacket = new Packet(*packet);
		writeSetCenterPacket(*setCenterPacket, server);
		server.getNetworkManager().getOutgoingQueue()->push(setCenterPacket);

		// 3. Send complete chunk batch sequence (Start -> Chunks -> Finished)
		sendChunkBatchSequence(*packet, server);

		// 4. Send player position and look - 0x41
		Packet* positionPacket = new Packet(*packet);
		sendPlayerPositionAndLook(*positionPacket, server);
		server.getNetworkManager().getOutgoingQueue()->push(positionPacket);

		// 5. Send spawn position - 0x5A
		Packet* spawnPacket = new Packet(*packet);
		sendSpawnPosition(*spawnPacket, server);
		server.getNetworkManager().getOutgoingQueue()->push(spawnPacket);

		// // 6. Complete spawn sequence (abilities, health, experience, time, held item)
		completeSpawnSequence(*packet, server);

		g_logger->logNetwork(INFO, "Complete game sequence sent to player: ", "PacketRouter");

		packet->setReturnPacket(PACKET_OK);
	} catch (const std::exception& e) {
		g_logger->logNetwork(ERROR, "Error in game sequence: " + std::string(e.what()), "PacketRouter");
		packet->setReturnPacket(PACKET_ERROR);
	}
}
