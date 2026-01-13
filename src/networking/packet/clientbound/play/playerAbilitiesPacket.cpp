#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"

void playerAbilitiesPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeByte(0x08);  // Invulnerable 0x01; Flying 0x02; Allow Flying 0x04; Creative Mode 0x08;
	buff.writeFloat(0.05); // Flight speed
	buff.writeFloat(1);	   // Fov modifier

	packet.sendPacket(PacketId::Play::Clientbound::PLAYER_ABILITIES, buff, server, false);
}
