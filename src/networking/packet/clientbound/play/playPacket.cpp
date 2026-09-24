#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

#include <string>

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
	buf.writeVarInt(server.getGameData().getSyncedId("minecraft:dimension_type", "minecraft:overworld")); // 10. Dimension Type
	buf.writeString("minecraft:overworld"); // 11. Dimension Name (Identifier)
	buf.writeInt64(1L); // 12. Hashed seed
	buf.writeUByte(static_cast<uint8_t>(player->getGameMode())); // 13. Game mode
	buf.writeByte(-1); // 14. Previous Game mode (Byte) | Undefined
	buf.writeBool(false); // 15. Is Debug
	buf.writeBool(true); // 16. Is Flat

	// 17. Has death location
	bool hasDeathLocation = false; // Set to true if player has died
	buf.writeBool(hasDeathLocation);

	// 18. Death dimension name (Optional - only if hasDeathLocation is true)
	// 19. Death location (Optional - only if hasDeathLocation is true)
	if (hasDeathLocation) {
		buf.writeString("minecraft:overworld"); // Death dimension name
		buf.writeInt64(0); // Death location as Position (packed long)
	}

	buf.writeVarInt(0); // 20. Portal cooldown
	buf.writeVarInt(63); // 21. Sea level
	buf.writeBool(false); // 22. Enforces Secure Chat

	packet.sendPacket(PacketId::Play::Clientbound::LOGIN, buf, server);
}
