#include "packet.hpp"
#include "server.hpp"

void packetRooter(Packet &packet, Server &server) {
    Player  *player;

    if (packet.getPlayer() == NULL)
    {
        if (packet.getSocketFd(). != -1)
        {
            
        }
    }


    player = packet.getPlayer();
    packet.getPlayer().getPlayerState() {

    }
    // switch(packet._player.getState()) {
    //     case PacketState::Handshake:
    //         handleHandshakePacket(packet, server);
    //         break;
    //     case PacketState::Login:
    //         handleLoginPacket(packet, server);
    //         break;
    //     case PacketState::Status:
    //         handleStatusPacket(packet, server);
    //         break;
    //     case PacketState::Play:
    //         handlePlayPacket(packet, server);
    //         break;
    //     default:
    //         // État inconnu
    //         break;
    // }
}