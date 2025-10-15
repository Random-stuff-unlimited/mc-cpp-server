#include "buffer.hpp"
#include "network/packet.hpp"
#include "server.hpp"

// If implementing data packs we should actually send datapack info with the loaded datapacks
void clientboundKnownPacksPacket(Packet& packet, Server& server) {
	Buffer buffer;

	buffer.writeVarInt(1);
	buffer.writeString("minecraft");
	buffer.writeString("core");
	buffer.writeString("1.21.5");

	packet.sendPacket(0x0E, buffer, server, true);
}
