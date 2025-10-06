#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include <iostream>

void handleLoginAcknowledged(Packet& packet, Server& server) {
    std::cout << "[Login] Login Acknowledged received - transitioning to Configuration state\n";
    
    Player* player = packet.getPlayer();
    if (!player) {
        std::cerr << "[Login] Error: No player associated with Login Acknowledged packet\n";
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }
    
    // The Login Acknowledged packet has no payload (0 bytes), so nothing to read
    
    // The client has acknowledged the login success, now we officially transition to Configuration state
    player->setPlayerState(PlayerState::Configuration);
    
    std::cout << "[Login] Player " << player->getPlayerName() 
              << " successfully acknowledged login - transitioned to Configuration state\n";
    
    // Just acknowledge the packet - don't send anything yet
    // The client will send Client Information (packet 0x03) next in Configuration state
    packet.setReturnPacket(PACKET_OK);
    
    (void)server; // Suppress unused parameter warning
}