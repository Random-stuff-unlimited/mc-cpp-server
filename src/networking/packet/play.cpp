#include "UUID.hpp"
#include "buffer.hpp"
#include "networking.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "server.hpp"

#include <iostream>
#include <string>

void writePlaytPacket(Packet& packet, Server& server) {
	std::cout << "=== Play packet write init ===\n";
	Player* player = packet.getPlayer();

	Buffer buf;
	buf.writeInt(player->getPlayerID());
	buf.writeByte(0x00);

	std::vector<std::string> dimensionNames = {"minecraft:overworld"};
	buf.writeIdentifierArray(dimensionNames);

	buf.writeVarInt(server.getServerSize());

	// server render distance and simulation distance
	buf.writeVarInt(5);
	buf.writeVarInt(5);

	buf.writeByte(0x00); // debug
	buf.writeByte(0x00); // respawn screen
	buf.writeByte(0x00);
	buf.writeVarInt(0);                     // Overworld
	buf.writeLong(0);                       // hashed seed
	buf.writeByte(static_cast<uint8_t>(0)); // gamemode
	buf.writeByte(static_cast<uint8_t>(0)); // previus gamemode

	// Fin du Join Game packet
	buf.writeByte(0x00); // Is Debug
	buf.writeByte(0x00); // Is Flat

	// Has death location
	buf.writeByte(0x00); // false = pas de dimension/position envoyée ensuite
	/*
	buf.writeBool(0x01);
	buf.writeString("minecraft:overworld"); // Death dimension
	buf.writeLong(encodePosition(x, y, z)); // Death position
	*/

	buf.writeVarInt(0);  // Portal cooldown
	buf.writeVarInt(63); // Sea level
	buf.writeByte(0x00); // Enforces secure chat

	int packetId         = 0x24;
	int packetIdSize     = packet.getVarintSize(packetId);
	int totalPayloadSize = packetIdSize + buf.getData().size();

	Buffer finalBuf;
	finalBuf.writeVarInt(totalPayloadSize);
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
