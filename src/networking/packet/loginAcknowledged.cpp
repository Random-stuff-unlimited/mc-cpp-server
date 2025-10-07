#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include "logger.hpp"
#include <iostream>

void handleLoginAcknowledged(Packet& packet, Server& server) {
    // g_logger->logNetwork(INFO, "Login Acknowledged received - transitioning to Configuration state", "Login");

    Player* player = packet.getPlayer();
    if (!player) {
        // g_logger->logNetwork(ERROR, "Error: No player associated with Login Acknowledged packet", "Login");
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    // The Login Acknowledged packet has no payload (0 bytes), so nothing to read

    // The client has acknowledged the login success, now we officially transition to Configuration state
    player->setPlayerState(PlayerState::Configuration);

    // g_logger->logNetwork(INFO, "Player " + player->getPlayerName() + " successfully acknowledged login - transitioned to Configuration state", "Login");

    // Just acknowledge the packet - don't send anything yet
    // The client will send Client Information (packet 0x03) next in Configuration state
    packet.setReturnPacket(PACKET_OK);

    (void)server; // Suppress unused parameter warning
}
