#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/inventory/Menu.hpp"

// Clicks in a menu (ServerGamePacketListenerImpl.handleContainerClick): the click is done on the server, the client
// says what it predicted for the slots it changed, and only what differs is sent back
void handleContainerClickPacket(Packet& packet, Server& server) {
	Buffer& data		= packet.getData();
	int		containerId = data.readVarInt();
	int		stateId		= data.readVarInt();
	int		slot		= data.readShort();
	int		button		= data.readByte();
	int		clickType	= data.readVarInt();
	std::vector<std::pair<int, HashedStack>> changed;
	int count = data.readVarInt();
	for (int i = 0; i < count && i < 128; i++) {
		int changedSlot = data.readShort();
		changed.emplace_back(changedSlot, HashedStack::read(data));
	}
	HashedStack carried = HashedStack::read(data);

	Player& player = *packet.getPlayer();
	Level&	level  = server.getLevel();
	Menu&	menu   = Menus::current(player, level);
	if (menu.containerId() != containerId) return;
	if (player.getGameMode() == GameMode::Spectator) {
		menu.sendAllDataToRemote();
		return;
	}
	if (!menu.stillValid() || !menu.isValidSlotIndex(slot) || clickType < 0 || clickType > 6) return;
	bool outdated = stateId != menu.stateId();
	menu.suppressRemoteUpdates();
	menu.clicked(slot, button, static_cast<ClickType>(clickType));
	for (const auto& [changedSlot, stack] : changed) menu.setRemoteSlotUnsafe(changedSlot, stack);
	menu.setRemoteCarried(carried);
	menu.resumeRemoteUpdates();
	if (outdated) {
		menu.broadcastFullState();
	} else {
		menu.broadcastChanges();
	}
}

// The client closed its screen (handleContainerClose)
void handleContainerClosePacket(Packet& packet, Server& server) {
	packet.getData().readVarInt(); // Container id
	Menus::doCloseContainer(*packet.getPlayer(), server.getLevel());
}
