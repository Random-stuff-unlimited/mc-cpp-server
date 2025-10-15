#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

void setCenterPacket(Packet& packet, Server& server) {

	Buffer buf;
	buf.writeVarInt(0);
	buf.writeVarInt(0);

	packet.sendPacket(0x57, buf, server, false);
}
