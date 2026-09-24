#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "world/World.hpp"

// Teleports the player to the world spawn
void synchronizePlayerPositionPacket(Packet& packet, Server& server) {
	const World::Spawn& spawn = server.getWorld().getSpawn();
	Buffer				buf;

	buf.writeVarInt(1); // Teleport id, echoed by Accept Teleportation
	buf.writeDouble(spawn.x);
	buf.writeDouble(spawn.y);
	buf.writeDouble(spawn.z);
	buf.writeDouble(0); // Velocity
	buf.writeDouble(0);
	buf.writeDouble(0);
	buf.writeFloat(0); // Yaw
	buf.writeFloat(0); // Pitch
	buf.writeInt(0);   // Flags: absolute position

	packet.sendPacket(PacketId::Play::Clientbound::PLAYER_POSITION, buf, server);
}
