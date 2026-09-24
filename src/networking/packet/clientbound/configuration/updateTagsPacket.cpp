#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"

void sendUpdateTags(Packet& packet, Server& server) {
	const std::vector<GameData::RegistryTags>& registries = server.getGameData().getTags();
	Buffer									   buf;

	buf.writeVarInt(static_cast<int32_t>(registries.size()));
	for (const GameData::RegistryTags& registry : registries) {
		buf.writeString(registry.registry);
		buf.writeVarInt(static_cast<int32_t>(registry.tags.size()));
		for (const auto& [tagName, ids] : registry.tags) {
			buf.writeString(tagName);
			buf.writeVarInt(static_cast<int32_t>(ids.size()));
			for (int id : ids) {
				buf.writeVarInt(id);
			}
		}
	}

	packet.sendPacket(PacketId::Configuration::Clientbound::UPDATE_TAGS, buf, server);
}
