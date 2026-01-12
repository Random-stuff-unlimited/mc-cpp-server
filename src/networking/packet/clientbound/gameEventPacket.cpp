#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"



void gameEventPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	buf.writeUByte(13);
	buf.writeFloat(0);

	packet.sendPacket(0x26, buf, server, true);
}
