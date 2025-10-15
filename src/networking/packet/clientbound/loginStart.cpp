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

	// Build Login Success packet payload
	Buffer payload;
	payload.writeUUID(uuid);
	payload.writeString(username);
	payload.writeVarInt(0); // properties length (no properties)

	// Debug: Log the raw payload bytes
	std::string payloadHex = "";
	for (size_t i = 0; i < payload.getData().size(); i++) {
		char hex[3];
		sprintf(hex, "%02x", payload.getData()[i]);
		payloadHex += hex;
		if (i < payload.getData().size() - 1) payloadHex += " ";
	}
	g_logger->logNetwork(INFO, "Login Success payload bytes: " + payloadHex, "Login");

	// Calculate total packet size (packet ID + payload)
	int packetId		   = 0x02;
	int payloadSize		   = payload.getData().size();
	int packetIdVarintSize = packet.getVarintSize(packetId);
	int totalPayloadSize   = packetIdVarintSize + payloadSize;

	Buffer final;
	final.writeVarInt(totalPayloadSize); // Total packet size
	final.writeVarInt(packetId);		 // Login Success packet ID (0x02)
	final.writeBytes(payload.getData()); // UUID + username + properties

	packet.getData() = final;
	packet.setReturnPacket(PACKET_SEND);
	packet.setPacketSize(final.getData().size());

	// Debug: Log the complete packet bytes
	std::string finalHex = "";
	for (size_t i = 0; i < final.getData().size(); i++) {
		char hex[3];
		sprintf(hex, "%02x", final.getData()[i]);
		finalHex += hex;
		if (i < final.getData().size() - 1) finalHex += " ";
	}
	g_logger->logNetwork(INFO, "Complete Login Success packet bytes: " + finalHex, "Login");

	// Don't transition to Configuration yet - wait for Login Acknowledged
	g_logger->logNetwork(INFO,
						 "Login Success sent for user: " + username + ", UUID: " + uuid.toString() +
								 ", packet size: " + std::to_string(final.getData().size()),
						 "Login");
	(void)server;
}
