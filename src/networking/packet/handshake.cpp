#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include "logger.hpp"

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
	g_logger->logNetwork(INFO, "Protocol=" + std::to_string(protocolVersion) + ", Addr=" + serverAddr + ", State=" + std::to_string(nextState), "Handshake");
	if (nextState == 1) {
		packet.getPlayer()->setPlayerState(PlayerState::Status);
		g_logger->logNetwork(INFO, "Status request - keeping in temp list", "Handshake");
	} else if (nextState == 2) {
		packet.getPlayer()->setPlayerState(PlayerState::Login);
		server.promoteTempPlayer(packet.getPlayer());
		g_logger->logNetwork(INFO, "Login attempt - player promoted to main list", "Handshake");
	} else {
		g_logger->logNetwork(INFO, "Status packet sent removed the player", "Handshake");
		packet.getPlayer()->setPlayerState(PlayerState::None);
		packet.setReturnPacket(PACKET_DISCONNECT);
	}
	(void)port;
}
