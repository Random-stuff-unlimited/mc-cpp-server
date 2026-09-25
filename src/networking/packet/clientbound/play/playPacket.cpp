#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/World.hpp"

#include <string>

// Part shared by Login (play) and Respawn (CommonPlayerSpawnInfo)
void writeSpawnInfo(Buffer& buf, Player& player, Server& server) {
	const World& world = server.getWorld();
	buf.writeVarInt(server.getGameData().getSyncedId("minecraft:dimension_type", world.getDimensionName())); // Dimension type
	buf.writeString(world.getDimensionName());							   // Dimension name
	buf.writeLong(1);													   // Hashed seed (biome noise on the client)
	buf.writeUByte(static_cast<uint8_t>(player.getGameMode()));			   // Game mode
	buf.writeByte(-1);													   // Previous game mode: none
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

void sendPlayPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	Buffer buf;

	buf.writeInt(player->getPlayerID()); // 1. Entity ID
	buf.writeBool(false); // 2. Is hardcore
	buf.writeVarInt(3); // 3. Dimension Names (Prefixed Array of Identifier) | Number of dimensions
	buf.writeString("minecraft:overworld");
	buf.writeString("minecraft:the_nether");
	buf.writeString("minecraft:the_end");
	buf.writeVarInt(server.getConfig().getServerSize());   // 4. Max Players
	buf.writeVarInt(server.getConfig().getViewDistance()); // 5. View Distance
	buf.writeVarInt(server.getConfig().getViewDistance()); // 6. Simulation Distance
	buf.writeBool(false); // 7. Reduced Debug Info
	buf.writeBool(true); // 8. Enable respawn screen
	buf.writeBool(false); // 9. Do limited crafting
	writeSpawnInfo(buf, *player, server); // 10-21: dimension, seed, game mode, death location...
	buf.writeBool(false); // 22. Enforces Secure Chat

	packet.sendPacket(PacketId::Play::Clientbound::LOGIN, buf, server);
}
