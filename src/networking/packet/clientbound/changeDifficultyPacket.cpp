#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/PacketIds.hpp"

void changeDifficultyPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeUByte(2);	  // 0 Peaceful; 1 Easy; 2 Normal; 3 Hard
	buff.writeBool(true); // Is Difficulty locked ?

	packet.sendPacket(PacketId::Play::Clientbound::CHANGE_DIFFICULTY, buff, server, false);
}
