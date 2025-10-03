#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <unistd.h>

void handleHandshakePacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x00) {
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}
	int protocolVersion    = packet.getData().readVarInt();
	std::string serverAddr = packet.getData().readString(255);
	uint16_t port          = packet.getData().readUShort();
	int nextState          = packet.getData().readVarInt();
	std::cout << "[Handshake] Protocol=" << protocolVersion << ", Addr=" << serverAddr
	          << ", State=" << nextState << "\n";
	if (nextState == 1) {
		packet.getPlayer()->setPlayerState(PlayerState::Status);
		std::cout << "Status request - keeping in temp list" << std::endl;
	} else if (nextState == 2) {
		packet.getPlayer()->setPlayerState(PlayerState::Login);
		server.promoteTempPlayer(packet.getPlayer());
		std::cout << "Login attempt - player promoted to main list" << std::endl;
	} else {
		std::cout << "Status packet sent removed the player" << std::endl;
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
	}
	(void)port;
}
