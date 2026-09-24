#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

// One Registry Data packet per synced registry. Entries are sent without data: the client takes it from
// the vanilla Known Pack. The client numbers entries in the order they are sent.
void sendRegistryData(Packet& packet, Server& server) {
	for (const GameData::Registry& registry : server.getGameData().getSyncedRegistries()) {
		Buffer buf;

		buf.writeString(registry.name);
		buf.writeVarInt(static_cast<int32_t>(registry.entries.size()));
		for (const std::string& entry : registry.entries) {
			buf.writeString(entry);
			buf.writeBool(false); // No data
		}

		packet.sendPacket(PacketId::Configuration::Clientbound::REGISTRY_DATA, buf, server);
	}
}
