#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

void setHeldItemPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeVarInt(packet.getPlayer()->getSelectedSlot()); // Hotbar slot 0-8

	packet.sendPacket(PacketId::Play::Clientbound::SET_HELD_SLOT, buff, server);
}
