#include "buffer.hpp"
#include "network/packet.hpp"

// If implementing data packs we should actually send datapack info with the loaded datapacks
void clientboundKnownPacks(Packet& packet) {
	Buffer buffer;

	buffer.writeByte(0x0E);
	buffer.writeVarInt(1);
	buffer.writeString("minecraft");
	buffer.writeString("core");
	buffer.writeString("1.21.5");

	Buffer final;
	final.writeVarInt(buffer.getData().size());
	final.writeBytes(buffer.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
