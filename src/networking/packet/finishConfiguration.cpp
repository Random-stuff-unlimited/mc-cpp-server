#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include <iostream>

void handleFinishConfiguration(Packet& packet, Server& server) {
    std::cout << "[Configuration] Starting configuration finish sequence\n";

    Player* player = packet.getPlayer();
    if (!player) {
        std::cerr << "[Configuration] Error: No player associated with packet\n";
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    std::cout << "[Configuration] Sending Finish Configuration packet to "
              << player->getPlayerName() << "\n";

    // Send Finish Configuration packet (0x03)
    Buffer payload;
    payload.writeVarInt(0x03); // Finish Configuration packet ID

    Buffer final;
    final.writeVarInt(payload.getData().size());
    final.writeBytes(payload.getData());

    packet.getData() = final;
    packet.setReturnPacket(PACKET_SEND);
    packet.setPacketSize(final.getData().size());

    std::cout << "[Configuration] Finish Configuration packet sent, waiting for client acknowledgment\n";

    (void)server; // Suppress unused parameter warning
}

void handleAcknowledgeFinishConfiguration(Packet& packet, Server& server) {
    std::cout << "[Configuration] Received Acknowledge Finish Configuration\n";

    Player* player = packet.getPlayer();
    if (!player) {
        std::cerr << "[Configuration] Error: No player associated with packet\n";
        packet.setReturnPacket(PACKET_DISCONNECT);
        return;
    }

    // Client has acknowledged finish configuration, now transition to Play state
    player->setPlayerState(PlayerState::Play);

    std::cout << "[Configuration] Player " << player->getPlayerName()
              << " transitioned to Play state - ready for game packets\n";

    // Just acknowledge the packet - the actual game sequence will be triggered separately
    packet.setReturnPacket(PACKET_OK);

    (void)server; // Suppress unused parameter warning
}
