#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include "logger.hpp"
#include <iostream>

void handleFinishConfiguration(Packet& packet, Server& server) {
    g_logger->logNetwork(INFO, "Starting configuration finish sequence", "Configuration");

    Player* player = packet.getPlayer();
    if (!player) {
        g_logger->logNetwork(ERROR, "Error: No player associated with packet", "Configuration");
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    g_logger->logNetwork(INFO, "Sending Finish Configuration packet to " + player->getPlayerName(), "Configuration");

    // Send Finish Configuration packet (0x03)
    Buffer payload;
    payload.writeVarInt(0x03); // Finish Configuration packet ID

    Buffer final;
    final.writeVarInt(payload.getData().size());
    final.writeBytes(payload.getData());

    packet.getData() = final;
    packet.setReturnPacket(PACKET_SEND);
    packet.setPacketSize(final.getData().size());

    g_logger->logNetwork(INFO, "Finish Configuration packet sent, waiting for client acknowledgment", "Configuration");

    (void)server; // Suppress unused parameter warning
}

void handleAcknowledgeFinishConfiguration(Packet& packet, Server& server) {
    g_logger->logNetwork(INFO, "Received Acknowledge Finish Configuration", "Configuration");

    Player* player = packet.getPlayer();
    if (!player) {
        g_logger->logNetwork(ERROR, "Error: No player associated with packet", "Configuration");
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    // Client has acknowledged finish configuration, now transition to Play state
    player->setPlayerState(PlayerState::Play);

    g_logger->logNetwork(INFO, "Player " + player->getPlayerName() + " transitioned to Play state - ready for game packets", "Configuration");

    // Just acknowledge the packet - the actual game sequence will be triggered separately
    packet.setReturnPacket(PACKET_OK);

    (void)server; // Suppress unused parameter warning
}
