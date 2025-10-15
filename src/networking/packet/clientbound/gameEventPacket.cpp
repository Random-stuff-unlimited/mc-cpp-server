#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

#include <string>

void gameEventPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	buf.writeByte(0x22);
	buf.writeByte(13);
	buf.writeFloat(0);


	Buffer final;

	final.writeVarInt(buf.getData().size());
	final.writeBytes(buf.getData());

	packet.setPacketId(0x22);
	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
	(void)server;
}
