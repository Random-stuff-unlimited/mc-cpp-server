#include "buffer.hpp"
#include "packet.hpp"

void changeDifficultyPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeByte(2);	  // 0 Peaceful; 1 Easy; 2 Normal; 3 Hard
	buff.writeBool(true); // Is Difficulty locked ?

	packet.sendPacket(0x0A, buff, server, false);
}
