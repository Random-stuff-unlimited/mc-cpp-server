#include "packet.hpp"
#include "server.hpp"
#include <iostream>

void handleConfirmTeleportation(Packet& packet, Server& server) {
	std::cout << "=== Received Confirm Teleportation ===\n";

	// Read teleport ID from packet data
	int teleportId = packet.getData().readVarInt();

	std::cout << "Player confirmed teleportation with ID: " << teleportId << std::endl;

	// Mark packet as processed
	packet.setReturnPacket(PACKET_OK);

	(void)server;
}
