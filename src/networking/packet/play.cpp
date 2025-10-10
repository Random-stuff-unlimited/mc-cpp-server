#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

#include <string>

void writePlayPacket(Packet& packet) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	// 1. Entity ID
	buf.writeInt(player->getPlayerID());

	// 2. Is Hardcore (false)
	buf.writeBool(false);

	// 3. Game mode (survival = 0)
	buf.writeByte(0);

	// 4. Previous game mode (-1 = not set)
	buf.writeByte(-1);

	// 5. World names (just overworld for now)
	buf.writeVarInt(1); // Number of worlds
	buf.writeString("minecraft:overworld");

	// 6. Registry codec (empty/minimal for now)
	// We'll write a minimal empty NBT structure
	buf.writeByte(0x0A); // TAG_Compound
	buf.writeByte(0);	 // Short value (high byte)
	buf.writeByte(0);	 // Short value (low byte)
	buf.writeByte(0x00); // TAG_End

	// 7. Dimension (empty/minimal for now)
	// We'll write a minimal empty NBT structure
	buf.writeByte(0x0A); // TAG_Compound
	buf.writeByte(0);	 // Short value (high byte)
	buf.writeByte(0);	 // Short value (low byte)
	buf.writeByte(0x00); // TAG_End

	// 8. World name
	buf.writeString("minecraft:overworld");

	// 9. Hashed seed
	buf.writeLong(12345678L);

	// 10. Max players
	buf.writeVarInt(20);

	// 11. View distance
	buf.writeVarInt(10);

	// 12. Simulation distance
	buf.writeVarInt(10);

	// 13. Reduced debug info
	buf.writeBool(false);

	// 14. Enable respawn screen
	buf.writeBool(true);

	// 15. Do limited crafting
	buf.writeBool(false);

	// 16. Dimension type
	buf.writeString("minecraft:overworld");

	// 17. Dimension name
	buf.writeString("minecraft:overworld");

	// 18. Portal cooldown
	buf.writeVarInt(0);

	// 19. Enforces secure chat
	buf.writeBool(false);

	packet.setPacketId(0x2B); // Login (play) packet ID
	packet.getData() = buf;
}
