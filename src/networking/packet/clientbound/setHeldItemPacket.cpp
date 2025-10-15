#include "buffer.hpp"
#include "packet.hpp"
#include "server.hpp"

void setHeldItemPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeVarInt(3); // 0-8 hand slots --> Should get it from player data when implemented

	packet.sendPacket(0x62, buff, server, false);
}
