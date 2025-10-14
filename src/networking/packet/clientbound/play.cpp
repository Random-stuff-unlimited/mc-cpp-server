#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"

#include <string>

void writePlayPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) return;

	Buffer buf;

	buf.writeByte(0x2B);

	// 1. Entity ID
	buf.writeInt(player->getPlayerID());

	// 2. Is hardcore
	buf.writeBool(false);

	// 3. Dimension Names (Prefixed Array of Identifier)
	buf.writeVarInt(3); // Number of dimensions
	buf.writeString("minecraft:overworld");
	buf.writeString("minecraft:the_nether");
	buf.writeString("minecraft:the_end");

	// 4. Max Players
	buf.writeVarInt(20);

	// 5. View Distance
	buf.writeVarInt(10);

	// 6. Simulation Distance
	buf.writeVarInt(10);

	// 7. Reduced Debug Info
	buf.writeBool(false);

	// 8. Enable respawn screen
	buf.writeBool(true);

	// 9. Do limited crafting
	buf.writeBool(false);

	// 10. Dimension Type (VarInt - registry ID)
	buf.writeVarInt(0); // overworld dimension type ID

	// 11. Dimension Name (Identifier)
	buf.writeString("minecraft:overworld");

	// 12. Hashed seed
	buf.writeLong(1L);

	// 13. Game mode (Unsigned Byte)
	buf.writeByte(1); // Creative

	// 14. Previous Game mode (Byte)
	buf.writeByte(-1); // Undefined

	// 15. Is Debug
	buf.writeBool(false);

	// 16. Is Flat
	buf.writeBool(true);

	// 17. Has death location
	bool hasDeathLocation = false; // Set to true if player has died
	buf.writeBool(hasDeathLocation);

	// 18. Death dimension name (Optional - only if hasDeathLocation is true)
	// 19. Death location (Optional - only if hasDeathLocation is true)
	if (hasDeathLocation) {
		buf.writeString("minecraft:overworld"); // Death dimension name
		buf.writeLong(0); // Death location as Position (packed long)
	}

	// 20. Portal cooldown
	buf.writeVarInt(0);

	// 21. Sea level
	buf.writeVarInt(63);

	// 22. Enforces Secure Chat
	buf.writeBool(false);

	Buffer final;

	final.writeVarInt(buf.getData().size());
	final.writeBytes(buf.getData());

	packet.setPacketId(0x2B);
	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
	(void)server;
}
