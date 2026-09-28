#include "PacketIds.hpp"
#include "buffer.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/networking.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

// The configuration packets the server sends. One file per state, by direction: every clientbound function of the
// Configuration state lives here.

// ClientboundKnownPacksPacket: the packs the server has, so the client knows what the entries of Registry Data refer to
void clientboundKnownPacksPacket(Packet& packet, Server& server) {
	Buffer buffer;

	buffer.writeVarInt(1);
	buffer.writeString("minecraft");
	buffer.writeString("core");
	buffer.writeString(server.getGameData().getVersionName());

	packet.sendPacket(PacketId::Configuration::Clientbound::SELECT_KNOWN_PACKS, buffer, server);
}

// ClientboundUpdateEnabledFeaturesPacket
void clientboundFeatureFlagsPacket(Packet& packet, Server& server) {
	Buffer buffer;

	buffer.writeVarInt(1);
	buffer.writeString("minecraft:vanilla");

	packet.sendPacket(PacketId::Configuration::Clientbound::UPDATE_ENABLED_FEATURES, buffer, server);
}

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

// ClientboundFinishConfigurationPacket: the configuration is over, the client can send Finish Configuration
void handleFinishConfigurationPacket(Packet& packet, Server& server) {
	Player* player = packet.getPlayer();
	if (!player) {
		packet.setReturnPacket(PACKET_DISCONNECT);
		return;
	}

	Buffer buf;

	packet.sendPacket(PacketId::Configuration::Clientbound::FINISH_CONFIGURATION, buf, server);
}