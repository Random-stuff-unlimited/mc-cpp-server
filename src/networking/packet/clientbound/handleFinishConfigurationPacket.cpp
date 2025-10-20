#include "buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void handleFinishConfigurationPacket(Packet& packet, Server& server) {
	// g_logger->logNetwork(INFO, "Starting configuration finish sequence", "Configuration");

	Player* player = packet.getPlayer();
	if (!player) {
		// g_logger->logNetwork(ERROR, "Error: No player associated with packet", "Configuration");
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	Buffer buf;

	packet.sendPacket(0x03, buf, server, true);
}
