#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/PluginChannels.hpp"
#include "network/packetRouter.hpp"
#include "network/TextComponent.hpp"
#include "lib/UUID.hpp"
#include "player.hpp"

// Network thread: the Configuration state. Client Information is kept, Select Known Packs sends the registries and
// ends the configuration
void handleConfigurationState(Packet* packet, Server& server) {
	switch (packet->getId()) {
	case PacketId::Configuration::Serverbound::CLIENT_INFORMATION:
		handleClientInformationPacket(*packet, server);
		sendServerResourcePack(*packet, server); // ServerResourcePackConfigurationTask
		break;
	case PacketId::Configuration::Serverbound::RESOURCE_PACK:
		handleResourcePackResponse(*packet, server);
		break;
	case PacketId::Configuration::Serverbound::SELECT_KNOWN_PACKS:
		serverboundKnownPacksPacket(*packet);
		sendRegistryData(*packet, server);
		sendUpdateTags(*packet, server);
		handleFinishConfigurationPacket(*packet, server);
		break;
	case PacketId::Configuration::Serverbound::COOKIE_RESPONSE:
		if (packet->getPlayer()) Cookies::handleResponse(*packet->getPlayer(), packet->getData());
		break;
	case PacketId::Configuration::Serverbound::CUSTOM_PAYLOAD:
		if (packet->getPlayer()) PluginChannels::handle(server, *packet->getPlayer(), packet->getData());
		break;
	case PacketId::Configuration::Serverbound::KEEP_ALIVE:
	case PacketId::Configuration::Serverbound::PONG:
	case PacketId::Configuration::Serverbound::CUSTOM_CLICK_ACTION:
		// Nothing to do yet
		break;
	default: {
		Buffer payload;
		payload.writeString("{\"text\":\"Unknown packet in Configuration state\"}");
		packet->sendPacket(PacketId::Configuration::Clientbound::DISCONNECT, payload, server);
		disconnect(packet);
		break;
	}
	}
}
// The configured resource pack (once per connection): its URL, hash, whether it is required and the prompt
void sendServerResourcePack(Packet& packet, Server& server) {
	const Config& config = server.getConfig();
	Player*		  player = packet.getPlayer();
	if (!player || config.getResourcePack().empty() || player->resourcePackSent()) return;
	player->setResourcePackSent(true);
	Buffer data;
	data.writeUUID(UUID::fromOfflinePlayer(config.getResourcePack())); // A stable id per URL
	data.writeString(config.getResourcePack());
	data.writeString(config.getResourcePackSha1());
	data.writeBool(config.getRequireResourcePack());
	data.writeBool(!config.getResourcePackPrompt().empty());
	if (!config.getResourcePackPrompt().empty()) TextComponent::writeJson(data, config.getResourcePackPrompt());
	packet.sendPacket(PacketId::Configuration::Clientbound::RESOURCE_PACK_PUSH, data, server);
}

// ServerboundResourcePackPacket: a required pack declined or failing disconnects the player
void handleResourcePackResponse(Packet& packet, Server& server) {
	packet.getData().readUUID();
	int status = packet.getData().readVarInt();
	// 1 declined, 2 failed download, 5 invalid URL, 6 failed reload, 7 discarded
	bool failed = status == 1 || status == 2 || status == 5 || status == 6 || status == 7;
	if (failed && server.getConfig().getRequireResourcePack()) server.kick(packet.getPlayer(), "multiplayer.requiredTexturePrompt.disconnect");
}
