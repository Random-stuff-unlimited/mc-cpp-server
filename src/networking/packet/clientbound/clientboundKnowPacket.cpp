#include "buffer.hpp"
#include "logger.hpp"
#include "network/packet.hpp"

void clientboundKnownPacks(Packet& packet) {
	Buffer buffer;

	buffer.writeVarInt(0x0E);
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
