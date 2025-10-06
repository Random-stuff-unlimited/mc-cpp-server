#include "UUID.hpp"
#include "buffer.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"
#include "logger.hpp"

#include <iostream>
#include <string>

void handleLoginStartPacket(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Login Start Received ===", "Login");
	Player* player = packet.getPlayer();
	if (!player) return;

	std::string username = packet.getData().readString(16);
	player->setPlayerName(username);

	UUID uuid = UUID::fromOfflinePlayer(username);
	player->setUUID(uuid);

	Buffer payload;
	payload.writeVarInt(0x02);
	payload.writeUUID(uuid);
	payload.writeString(username);
	payload.writeVarInt(0);

	Buffer final;
	final.writeVarInt(payload.getData().size());
	final.writeBytes(payload.getData());

	packet.getData() = final;
	packet.setReturnPacket(PACKET_SEND);
	packet.setPacketSize(final.getData().size());
	// Don't transition to Configuration yet - wait for Login Acknowledged
	g_logger->logNetwork(INFO, "Login Success sent, waiting for Login Acknowledged", "Login");
	(void)server;
}
