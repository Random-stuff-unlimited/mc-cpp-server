#include "lib/UUID.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"

#include <iostream>

void writeSetCenterPacket(Packet& packet) {
	std::cout << "=== center chunk packet write init ===\n";

	Buffer buff;
	buff.writeByte(0x57);
	buff.writeVarInt(0);
	buff.writeVarInt(0);

	Buffer final;
	final.writeVarInt(buff.getData().size());
	final.writeBytes(buff.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
