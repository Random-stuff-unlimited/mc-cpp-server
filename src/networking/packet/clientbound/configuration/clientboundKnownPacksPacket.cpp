#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

// If implementing data packs we should actually send datapack info with the loaded datapacks
void clientboundKnownPacksPacket(Packet& packet, Server& server) {
	Buffer buffer;

	buffer.writeVarInt(1);
	buffer.writeString("minecraft");
	buffer.writeString("core");
	buffer.writeString("1.21.10");

	packet.sendPacket(PacketId::Configuration::Clientbound::KNOWN_PACKS, buffer, server, true);
}
