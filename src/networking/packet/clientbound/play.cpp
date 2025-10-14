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

	// 2. Is Hardcore (false)
	buf.writeBool(false);

	// 3. Dimension Names (Prefixed Array)
	buf.writeVarInt(3);
	buf.writeString("minecraft:overworld");
	buf.writeString("minecraft:the_nether");
	buf.writeString("minecraft:the_end");

	// 4. Max PLayer
	buf.writeVarInt(server.getConfig().getServerSize());

	// 5. view distance
	buf.writeVarInt(5);

	// 6. Reduced debug info
	buf.writeBool(false);

	// 7. Enable respawn screen
	buf.writeBool(true);

	// 8. Do limited crafting
	buf.writeBool(false);

	// 9. Dimension type
	buf.writeVarInt(1);

	// 10. Dimension name
	buf.writeString("minecraft:overworld");

	// 11. Hashed seed
	buf.writeLong(12345678L);

	// 12. hashed seed
	buf.writeLong(12345678L);

	// 12. gamemode
	buf.writeByte(0);

	// 13. previous gamemode
	buf.writeByte(0);

	// 14. is debug
	buf.writeBool(false);

	// 15. is flat
	buf.writeBool(false);

	// 16. has death location
	buf.writeBool(false);

	// 17. has death location
	buf.writeBool(false);

	// 18. death dimension
	buf.writeString("minecraft:overworld");

	// 19. death location
	buf.writeDouble(0.0);
	buf.writeDouble(0.0);
	buf.writeDouble(0.0);

	// 20. portal cooldown
	buf.writeVarInt(0);

	// 21. sea level
	buf.writeVarInt(64);

	// 22. enforces secure chat
	buf.writeBool(false);

	Buffer final;

	final.writeInt(buf.getData().size());
	final.writeBytes(buf.getData());

	packet.setPacketId(0x2B);
	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
