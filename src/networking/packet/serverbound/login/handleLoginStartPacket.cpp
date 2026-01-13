#include "lib/UUID.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <string>

void handleLoginStartPacket(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Login Start Received ===", "Login");
	Player* player = packet.getPlayer();
	if (!player) return;

	std::string username = packet.getData().readString(16);
	player->setPlayerName(username);

	UUID uuid = UUID::fromOfflinePlayer(username);
	player->setUUID(uuid);

	Buffer buff;
	buff.writeUUID(uuid);
	buff.writeString(username);
	buff.writeVarInt(0); // properties lenght (no properties)

	packet.sendPacket(0x02, buff, server, true);
}
