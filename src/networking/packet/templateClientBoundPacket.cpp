#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

void templateClientBoundPacket(Packet& packet, Server& server) {
	Buffer buff;

	// Add packet data here

	packet.sendPacket(0x00, buff, server, false); // Replace 0x00 with actual packet ID
}