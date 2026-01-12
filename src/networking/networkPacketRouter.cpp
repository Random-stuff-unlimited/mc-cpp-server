#include "logger.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "network/packetRouter.hpp"
#include "player.hpp"

#include <string>

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
		if (packet->getSize() > 32767) {
			g_logger->logNetwork(ERROR, "Packet size too large: " + std::to_string(packet->getSize()), "PacketRouter");
			packet->setReturnPacket(PACKET_DISCONNECT);
			return;
		}
		if (packet->getId() == 0x00) {
			handleLoginStartPacket(*packet, server);
		// } else if (packet->getId() == 0x02) {
		// 	g_logger->logNetwork(INFO, "Received Login Plugin Response (0x02) - acknowledging", "PacketRouter");
		// 	packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x03) {
			handleLoginAcknowledgedPacket(*packet, server);
			clientboundKnownPacksPacket(*packet, server);
		// } else if (packet->getId() == 0x04) {
		// 	g_logger->logNetwork(INFO, "Received Login Cookie Response (0x04) - acknowledging", "PacketRouter");
		// 	packet->setReturnPacket(PACKET_OK);
		} else {
			packet->getPlayer()->setPlayerState(PlayerState::None);
			packet->setReturnPacket(PACKET_DISCONNECT);
		}
		break;
	case PlayerState::Configuration:
		if (packet->getId() == 0x00) {
			// Client Information
			handleClientInformationPacket(*packet, server);
		} else if (packet->getId() == 0x01) {
			// Cookie Response
			packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x02) {
			// Serverbound Plugin Message
			packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x03) {
			// Acknowledge Finish Configuration -> Enter Play State
			g_logger->logNetwork(INFO, "Transitioning to Play state", "Configuration");
			handleAcknowledgeFinishConfigurationPacket(*packet, server);

			// Send play initialization packets
			sendPlayPacket(*packet, server);
			changeDifficultyPacket(*packet, server);
			playerAbilitiesPacket(*packet, server);
			setHeldItemPacket(*packet, server);
			synchronizePlayerPositionPacket(*packet, server); // Last packet
		} else if (packet->getId() == 0x04) {
			// Keep Alive
			packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x05) {
			// Pong
			packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x06) {
			// Resource Pack Response
			packet->setReturnPacket(PACKET_OK);
		} else if (packet->getId() == 0x07) {
			// Serverbound Known Packs -> Send Configuration Data
			serverboundKnownPacksPacket(*packet);

			// Send configuration sequence
			g_logger->logNetwork(INFO, "Sending Registry Data", "Configuration");
			sendRegistryData(*packet, server);

			// g_logger->logNetwork(INFO, "Sending Update Tags", "Configuration");
			// sendUpdateTags(*packet, server);

			g_logger->logNetwork(INFO, "Sending Finish Configuration", "Configuration");
			handleFinishConfigurationPacket(*packet, server);
		} else if (packet->getId() == 0x08) {
			// Custom Click Action
			packet->setReturnPacket(PACKET_OK);
		} else {
			// Unknown packet - disconnect
			Buffer payload;
			payload.writeString("{\"text\":\"Unknown packet in Configuration state\"}");
			packet->sendPacket(0x02, payload, server, true);
			packet->getPlayer()->setPlayerState(PlayerState::None);
			packet->setReturnPacket(PACKET_DISCONNECT);
		}
		break;
	case PlayerState::Play:
		if (packet->getId() == 0x00) {
			// Confirm Teleportation
			handleConfirmTeleportationPacket(*packet, server);
			gameEventPacket(*packet, server);
		} else if (packet->getId() == 0x2B) {
			// Player Loaded
			g_logger->logNetwork(DEBUG, "Player fully loaded in game", "Play");
			packet->setReturnPacket(PACKET_OK);
		} else {
			// Other play packets
			packet->setReturnPacket(PACKET_OK);
		}
		break;
	default:
		g_logger->logNetwork(WARN, "Unknown player state: " + std::to_string(static_cast<int>(player->getPlayerState())), "PacketRouter");
		packet->setReturnPacket(PACKET_DISCONNECT);
		break;
	}
}
