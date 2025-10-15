#include "buffer.hpp"
#include "packet.hpp"

void templateClientBoundPacket(Packet& packet) {
	Buffer buff;

	// Add packet id, and data.

	Buffer final;
	final.writeVarInt(buff.getData().size());
	final.writeBytes(buff.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
