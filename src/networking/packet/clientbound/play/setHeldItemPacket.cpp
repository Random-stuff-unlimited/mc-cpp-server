#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

void setHeldItemPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeVarInt(3); // 0-8 hand slots --> Should get it from player data when implemented

	packet.sendPacket(PacketId::Play::Clientbound::SET_HELD_ITEM, buff, server, false);
}
