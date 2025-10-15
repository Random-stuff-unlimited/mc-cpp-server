#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

#include <string>

void gameEventPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	buf.writeByte(13);
	buf.writeFloat(0);

	packet.sendPacket(0x22, buf, server, false);
}
