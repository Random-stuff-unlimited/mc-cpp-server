#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

void playerAbilitiesPacket(Packet& packet, Server& server) {
	Buffer	 buff;
	GameMode mode  = packet.getPlayer()->getGameMode();
	uint8_t	 flags = 0; // Invulnerable 0x01; Flying 0x02; Allow Flying 0x04; Instant break 0x08
	if (mode == GameMode::Creative) flags = 0x01 | 0x04 | 0x08;
	if (mode == GameMode::Spectator) flags = 0x01 | 0x02 | 0x04;

	buff.writeUByte(flags);
	buff.writeFloat(0.05); // Flight speed
	buff.writeFloat(0.1f); // Walking speed, vanilla 0.1. The client zooms the FOV by speed / this value, so 1 looked like Slowness

	packet.sendPacket(PacketId::Play::Clientbound::PLAYER_ABILITIES, buff, server);
}
