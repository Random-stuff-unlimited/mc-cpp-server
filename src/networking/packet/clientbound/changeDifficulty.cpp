#include "buffer.hpp"
#include "packet.hpp"

void changeDifficulty(Packet& packet) {
	Buffer buff;

	buff.writeByte(0x0A);
	buff.writeByte(2);	  // 0 Peaceful; 1 Easy; 2 Normal; 3 Hard
	buff.writeBool(true); // Is Difficulty locked ?

	Buffer final;
	final.writeVarInt(buff.getData().size());
	final.writeBytes(buff.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
