#include "PacketIds.hpp"
#include "lib/UUID.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <string>

// The login packets the server receives. One file per state, by direction.

// ServerboundLoginStartPacket: the player gives its name, the server answers with the compression and its UUID
void handleLoginStartPacket(Packet& packet, Server& server) {
	g_logger->logNetwork(INFO, "=== Login Start Received ===", "Login");
	Player* player = packet.getPlayer();
	if (!player) return;

	std::string username = packet.getData().readString(16);

	// Same name = same offline UUID: both clients would take the other for themselves. Like vanilla, the new
	// connection wins and the old one is kicked
	for (const auto& existing : server.findPlayersByName(username)) {
		if (existing.get() != player) server.kick(existing.get(), "multiplayer.disconnect.duplicate_login");
	}
	player->setPlayerName(username);

	UUID uuid = UUID::fromOfflinePlayer(username);
	player->setUUID(uuid);

	// Set Compression goes out uncompressed; every packet after it, both ways, uses the compressed format
	int threshold = server.getConfig().getCompressionThreshold();
	if (threshold >= 0) {
		Buffer compression;
		compression.writeVarInt(threshold);
		packet.sendPacket(PacketId::Login::Clientbound::LOGIN_COMPRESSION, compression, server);
		player->setCompressionThreshold(threshold);
	}

	Buffer buff;
	buff.writeUUID(uuid);
	buff.writeString(username);
	buff.writeVarInt(0); // properties lenght (no properties)

	packet.sendPacket(PacketId::Login::Clientbound::LOGIN_FINISHED, buff, server);
}

// ServerboundLoginAcknowledgedPacket: the client acknowledges its login, the state becomes Configuration
void handleLoginAcknowledgedPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}
	player->setPlayerState(PlayerState::Configuration);
	packet.setReturnPacket(PACKET_OK);
	(void)server;
}