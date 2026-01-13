#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/PacketIds.hpp"

void clientboundFeatureFlagsPacket(Packet& packet, Server& server) {
	Buffer buffer;

	buffer.writeVarInt(1);
	buffer.writeString("minecraft:vanilla");

	packet.sendPacket(PacketId::Configuration::Clientbound::FEATURE_FLAGS, buffer, server, false);
}
