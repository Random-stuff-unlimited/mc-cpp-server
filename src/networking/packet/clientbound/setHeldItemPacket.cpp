#include "buffer.hpp"
#include "packet.hpp"

void setHeldItemPacket(Packet& packet) {
	Buffer buff;

	buff.writeByte(0x62);
	buff.writeVarInt(3); // 0-8 hand slots --> Should get it from player data when implemented

	Buffer final;
	final.writeVarInt(buff.getData().size());
	final.writeBytes(buff.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
