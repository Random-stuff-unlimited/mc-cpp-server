#include "buffer.hpp"
#include "packet.hpp"

void playerAbilities(Packet& packet) {
	Buffer buff;

	buff.writeByte(0x39);
	buff.writeByte(0x08);  // Invulnerable 0x01; Flying 0x02; Allow Flying 0x04; Creative Mode 0x08;
	buff.writeFloat(0.05); // Flight speed
	buff.writeFloat(1);	   // Fov modifier

	Buffer final;
	final.writeVarInt(buff.getData().size());
	final.writeBytes(buff.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
