#include "enums.hpp"
#include "packet.hpp"
#include "server.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <unistd.h>

void handleHandshakePacket(Packet& packet, Server& server) {
	if (packet.getId() != 0x00) // not handshake packet
	{
		packet.getPlayer()->setPlayerState(PlayerState::None);
		close(packet.getSocket());
		server.removePlayer(packet.getPlayer());
		return;
	}
	int protocolVersion    = packet.getData().readVarInt();
	std::string serverAddr = packet.getData().readString(255);
	uint16_t port =
	        packet.getData()
	                .readUShort(); // add check if the port is valid (same as the server port)
	int nextState =
	        packet.getData()
	                .readVarInt(); // add check if the address is valid (same as the server address)
	std::cout << "[Handshake] Protocol=" << protocolVersion << ", Addr=" << serverAddr
	          << ", State=" << nextState << "\n";
	if (nextState == 1) {
		packet.getPlayer()->setPlayerState(PlayerState::Status);
		std::cout << "Status packet sent status 1" << std::endl;
	} else if (nextState == 2) {
		packet.getPlayer()->setPlayerState(PlayerState::Login);
		std::cout << "Status packet status 2" << std::endl;
	} else {
		std::cout << "Status packet sent removed the player" << std::endl;
		packet.getPlayer()->setPlayerState(PlayerState::None);
		close(packet.getSocket());
		server.removePlayer(packet.getPlayer());
	}
	(void)port;
}
