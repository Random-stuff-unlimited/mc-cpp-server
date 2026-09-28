#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Survival.hpp"
#include "world/World.hpp"

#include <string>

// The play packets the server sends. One file per state, by direction: every clientbound function of the Play state
// lives here. The movement and entity packets are built where they are sent (PlayerTracker, EntityManager...).

// ChangeDifficultyPacket
void changeDifficultyPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeUByte(static_cast<uint8_t>(Survival::difficulty(server))); // 0 Peaceful; 1 Easy; 2 Normal; 3 Hard (config "difficulty")
	buff.writeBool(true);												// Is Difficulty locked ?

	packet.sendPacket(PacketId::Play::Clientbound::CHANGE_DIFFICULTY, buff, server);
}

// GameEventPacket: level events like rain (13 is the "game mode is changed" one)
void gameEventPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	buf.writeUByte(13);
	buf.writeFloat(0);

	packet.sendPacket(PacketId::Play::Clientbound::GAME_EVENT, buf, server);
}

// PlayerAbilitiesPacket: invulnerable, flying, allow flying and instant break flags, and the speeds
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

// SetHeldItemPacket: the hotbar slot the player selected
void setHeldItemPacket(Packet& packet, Server& server) {
	Buffer buff;

	buff.writeVarInt(packet.getPlayer()->getSelectedSlot()); // Hotbar slot 0-8

	packet.sendPacket(PacketId::Play::Clientbound::SET_HELD_SLOT, buff, server);
}

// Part shared by Login (play) and Respawn (CommonPlayerSpawnInfo)
void writeSpawnInfo(Buffer& buf, Player& player, Server& server) {
	const World& world = server.getWorld();
	buf.writeVarInt(server.getGameData().getSyncedId("minecraft:dimension_type", world.getDimensionName())); // Dimension type
	buf.writeString(world.getDimensionName());							   // Dimension name
	buf.writeLong(1);													   // Hashed seed (biome noise on the client)
	buf.writeUByte(static_cast<uint8_t>(player.getGameMode()));			   // Game mode
	buf.writeByte(static_cast<int8_t>(player.getPreviousGameMode()));	   // Previous game mode, -1 if none
	buf.writeBool(false);												   // Debug world
	buf.writeBool(true);												   // Flat world (lower horizon)

	// Death location, used by the recovery compass
	const CombatState& combat = player.combat();
	buf.writeBool(combat.hasDeathLocation);
	if (combat.hasDeathLocation) {
		buf.writeString(world.getDimensionName());
		buf.writePosition(combat.deathX, combat.deathY, combat.deathZ);
	}

	buf.writeVarInt(0);	 // Portal cooldown
	buf.writeVarInt(63); // Sea level
}

// Login (play) packet: everything the client needs to enter the world
void sendPlayPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	Buffer buf;

	buf.writeInt(player->getPlayerID()); // 1. Entity ID
	buf.writeBool(false);				// 2. Is hardcore
	buf.writeVarInt(3);					// 3. Dimension Names (Prefixed Array of Identifier) | Number of dimensions
	buf.writeString("minecraft:overworld");
	buf.writeString("minecraft:the_nether");
	buf.writeString("minecraft:the_end");
	buf.writeVarInt(server.getConfig().getServerSize());   // 4. Max Players
	buf.writeVarInt(server.getConfig().getViewDistance()); // 5. View Distance
	buf.writeVarInt(server.getConfig().getViewDistance()); // 6. Simulation Distance
	buf.writeBool(false);								  // 7. Reduced Debug Info
	buf.writeBool(true);								  // 8. Enable respawn screen
	buf.writeBool(false);								  // 9. Do limited crafting
	writeSpawnInfo(buf, *player, server);				  // 10-21: dimension, seed, game mode, death location...
	buf.writeBool(false);								  // 22. Enforces Secure Chat

	packet.sendPacket(PacketId::Play::Clientbound::LOGIN, buf, server);
}

// SynchronizePlayerPositionPacket: teleports the player to its position (the world spawn, or where it was saved)
void synchronizePlayerPositionPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	Buffer	 buf;

	buf.writeVarInt(player->nextTeleportId()); // Teleport id, echoed by Accept Teleportation
	buf.writeDouble(player->getX());
	buf.writeDouble(player->getY());
	buf.writeDouble(player->getZ());
	buf.writeDouble(0); // Velocity
	buf.writeDouble(0);
	buf.writeDouble(0);
	buf.writeFloat(player->getYaw());
	buf.writeFloat(player->getPitch());
	buf.writeInt(0); // Flags: absolute position

	packet.sendPacket(PacketId::Play::Clientbound::PLAYER_POSITION, buf, server);
}