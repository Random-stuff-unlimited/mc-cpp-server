#include "buffer.hpp"
#include "packet.hpp"

void setHeldItem(Packet& packet) {
	Buffer buff;

	buff.writeByte(0x62);
	buff.writeVarInt(3);

	Buffer final;
	final.writeVarInt(buff.getData().size());
	final.writeBytes(buff.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
