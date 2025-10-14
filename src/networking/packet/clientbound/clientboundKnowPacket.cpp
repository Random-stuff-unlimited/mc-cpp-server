#include "buffer.hpp"
#include "logger.hpp"
#include "network/packet.hpp"

void clientboundKnownPacks(Packet& packet) {
	Buffer buffer;

	buffer.writeByte(0x0E);
	buffer.writeVarInt(1);
	buffer.writeString("minecraft");
	buffer.writeString("core");
	buffer.writeString("1.21.5");
	std::cout << buffer.getData().size() << std::endl;

	Buffer final;
	final.writeVarInt(buffer.getData().size());
	final.writeBytes(buffer.getData());

	packet.getData() = final;
	packet.setPacketSize(final.getData().size());
	packet.setReturnPacket(PACKET_SEND);
}
