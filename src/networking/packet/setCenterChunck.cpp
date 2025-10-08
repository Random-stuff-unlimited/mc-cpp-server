#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "network/server.hpp"

#include <iostream>

void writeSetCenterPacket(Packet& packet, Server& server) {
	std::cout << "=== center chunk packet write init ===\n";

	Buffer buf;
	buf.writeVarInt(0);
	buf.writeVarInt(0);

	int packetId         = 0x57;
	int packetIdSize     = packet.getVarintSize(packetId);
	int totalPayloadSize = packetIdSize + buf.getData().size();

	Buffer finalBuf;
	finalBuf.writeVarInt(totalPayloadSize);
	finalBuf.writeVarInt(packetId);
	finalBuf.writeBytes(buf.getData());

	packet.getData() = finalBuf;
	packet.setPacketSize(finalBuf.getData().size());
	packet.setReturnPacket(PACKET_SEND);

	(void)server;
}
