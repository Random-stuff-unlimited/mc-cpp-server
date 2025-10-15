#include "packet.hpp"
#include "server.hpp"

void handleConfirmTeleportationPacket(Packet& packet, Server& server) {
	// int teleportId = packet.getData().readVarInt();
	packet.setReturnPacket(PACKET_OK);
	(void)server;
}
