#include "buffer.hpp"
#include "packet.hpp"

void playerAbilitiesPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeByte(0x08);  // Invulnerable 0x01; Flying 0x02; Allow Flying 0x04; Creative Mode 0x08;
	buff.writeFloat(0.05); // Flight speed
	buff.writeFloat(1);	   // Fov modifier

	packet.sendPacket(0x39, buff, server, false);
}
