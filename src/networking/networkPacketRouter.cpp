#include "logger.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "server.hpp"
#include "player.hpp"
#include "packetRouter.hpp"

#include <string>

// ========================================
// Main Packet Router
// ========================================

void packetRouter(Packet* packet, Server& server) {
    // Basic validation checks
    if (packet == nullptr)
        return;

    if (server.getNetworkManager().getOutgoingQueue() == nullptr)
        return;

    // Get player and validate
    Player* player = packet->getPlayer();
    if (player == nullptr) {
        packet->setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    // Route packet based on player's current state
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
                WARN,
                "Unknown player state: " + std::to_string(static_cast<int>(player->getPlayerState())) + ", disconnecting",
                "PacketRouter"
            );
            packet->setReturnPacket(PACKET_DISCONNECT);
            break;
    }
}

// ========================================
// Handshake State Handler
// ========================================

void handleHandshakeState(Packet* packet, Server& server) {
    handleHandshakePacket(*packet, server);
}

// ========================================
// Status State Handler
// ========================================

void handleStatusState(Packet* packet, Server& server) {
    switch (packet->getId()) {
        case 0x00:
            // Status request packet
            handleStatusPacket(*packet, server);
            break;

        case 0x01:
            // Ping packet
            handlePingPacket(*packet, server);
            break;

        default:
            // Unknown packet in status state - disconnect
            // g_logger->logNetwork(WARN, "Unknown packet ID in Status state: 0x" + std::to_string(packet->getId()), "PacketRouter");
            packet->getPlayer()->setPlayerState(PlayerState::None);
            packet->setReturnPacket(PACKET_DISCONNECT);
            break;
    }
}

// ========================================
// Login State Handler
// ========================================

void handleLoginState(Packet* packet, Server& server) {
    // Check packet size limit
    if (packet->getSize() > 32767) {
        g_logger->logNetwork(ERROR, "Packet size too large: " + std::to_string(packet->getSize()), "PacketRouter");
        packet->setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    switch (packet->getId()) {
        case 0x00:
            // Login start packet
            handleLoginStartPacket(*packet, server);
            break;

        case 0x02:
            // Login plugin response
            g_logger->logNetwork(INFO, "Received Login Plugin Response (0x02) - acknowledging", "PacketRouter");
            packet->setReturnPacket(PACKET_OK);
            break;

        case 0x03:
            // Login acknowledged packet
            handleLoginAcknowledgedPacket(*packet, server);
            if (packet->getReturnPacket() == PACKET_DISCONNECT)
                return;
            clientboundKnownPacksPacket(*packet, server);
            break;

        case 0x04:
            // Login cookie response
            g_logger->logNetwork(INFO, "Received Login Cookie Response (0x04) - acknowledging", "PacketRouter");
            packet->setReturnPacket(PACKET_OK);
            break;

        default:
            // Unknown packet in login state - disconnect
            packet->getPlayer()->setPlayerState(PlayerState::None);
            packet->setReturnPacket(PACKET_DISCONNECT);
            break;
    }
}

// ========================================
// Configuration State Handler
// ========================================

void handleConfigurationState(Packet* packet, Server& server) {
    switch (packet->getId()) {
        case 0x00:
            // Client information packet
            g_logger->logNetwork(INFO, "Received Client Information in Configuration state", "Configuration");
            handleClientInformationPacket(*packet, server);
            break;

        case 0x01:
            // Cookie response in configuration
            g_logger->logNetwork(INFO, "Received Cookie Response in Configuration state", "Configuration");
            packet->setReturnPacket(PACKET_OK);
            break;

        case 0x02:
            // Serverbound plugin message
            g_logger->logNetwork(
                INFO,
                "Received Serverbound Plugin Message (0x02), size: " + std::to_string(packet->getSize()) + " bytes",
                "PacketRouter"
            );
            packet->setReturnPacket(PACKET_OK);
            break;

        case 0x03:
            // Acknowledge finish configuration - transition to play state
            handleAcknowledgeFinishConfigurationPacket(*packet, server);
            sendPlayPacket(*packet, server);                      // 1. Send Login (play) packet - 0x2B
            changeDifficultyPacket(*packet, server);              // 2. Send Change Difficulty - 0x42
            playerAbilitiesPacket(*packet, server);               // 3. Send Player Abilities - 0x39
            setHeldItemPacket(*packet, server);                   // 4. Set held item
            synchronizePlayerPositionPacket(*packet, server);     // 5. Send player position and look - 0x41
            break;

        case 0x04:
            // Keep alive in configuration
            g_logger->logNetwork(INFO, "Received Keep Alive in Configuration state", "Configuration");
            packet->setReturnPacket(PACKET_OK);
            break;

        case 0x05:
            // Pong in configuration
            g_logger->logNetwork(INFO, "Received Pong in Configuration state", "Configuration");
            packet->setReturnPacket(PACKET_OK);
            break;

        case 0x06:
            // Resource pack response
            g_logger->logNetwork(INFO, "Received Resource Pack Response in Configuration state", "Configuration");
            packet->setReturnPacket(PACKET_OK);
            break;

        case 0x07:
            // Serverbound known packs - finalize configuration
            g_logger->logNetwork(INFO, "Received Serverbound Known Packs in Configuration state", "Configuration");
            serverboundKnownPacksPacket(*packet);
            packet->setReturnPacket(PACKET_OK);
            sendRegistryData(*packet, server);    // Send registry data
            sendUpdateTags(*packet, server);      // Send update tags
            handleFinishConfigurationPacket(*packet, server);
            break;

        default:
            // Unknown packet in configuration state - disconnect
            packet->getPlayer()->setPlayerState(PlayerState::None);
            packet->setReturnPacket(PACKET_DISCONNECT);
            break;
    }
}

// ========================================
// Play State Handler
// ========================================

void handlePlayState(Packet* packet, Server& server) {
    switch (packet->getId()) {
        case 0x00:
            // Confirm teleportation packet
            handleConfirmTeleportationPacket(*packet, server);
            gameEventPacket(*packet, server);     // Send Game Event packet - 0x42
            setCenterPacket(*packet, server);     // Send Set Center Chunk - 0x57
            // TODO: Send Level Chunk With Light - 0x22
            packet->setReturnPacket(PACKET_OK);   // temp
            break;

        default:
            // Other play state packets - acknowledge for now
            packet->setReturnPacket(PACKET_OK);
            break;
    }
}
