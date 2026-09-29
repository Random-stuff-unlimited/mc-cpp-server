#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/inventory/Menu.hpp"
#include "world/inventory/ProcessingMenus.hpp"
#include "world/inventory/StorageMenus.hpp"

// The play packets the server receives. One file per state, by direction: the container clicks, the recipe book and
// the teleportation acknowledgement. Block interactions (digging, placing, using) are in blockInteractionPackets.cpp

// ServerboundAcceptTeleportationPacket: the client confirms it is at the position sent by Synchronize Player Position
void handleConfirmTeleportationPacket(Packet& packet, Server& server) {
	packet.getData().readVarInt(); // Teleport id
	packet.setReturnPacket(PACKET_OK);
	(void)server;
}

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
	Level&	level  = server.levelOf(*packet.getPlayer());
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
	Menus::doCloseContainer(*packet.getPlayer(), server.levelOf(*packet.getPlayer()));
}

// A button of the menu (handleContainerButtonClick): a lectern's pages, taking its book...
void handleContainerButtonClickPacket(Packet& packet, Server& server) {
	Buffer& data		= packet.getData();
	int		containerId = data.readVarInt();
	int		button		= data.readVarInt();
	Player& player		= *packet.getPlayer();
	Menu&	menu		= Menus::current(player, server.levelOf(*packet.getPlayer()));
	if (menu.containerId() != containerId || player.getGameMode() == GameMode::Spectator || !menu.stillValid()) return;
	if (menu.clickMenuButton(button)) menu.broadcastChanges();
}

// A crafter's slot toggled (handleContainerSlotStateChanged)
void handleContainerSlotStateChangedPacket(Packet& packet, Server& server) {
	Buffer& data		= packet.getData();
	int		slot		= data.readVarInt();
	int		containerId = data.readVarInt();
	bool	enabled		= data.readBool();
	Player& player		= *packet.getPlayer();
	Menu&	menu		= Menus::current(player, server.levelOf(*packet.getPlayer()));
	if (player.getGameMode() == GameMode::Spectator || menu.containerId() != containerId) return;
	if (auto* crafter = dynamic_cast<CrafterMenu*>(&menu)) crafter->crafter().setSlotState(slot, enabled);
}

// The recipe book. Recipes aren't unlocked by advancements yet (there are none): every player knows them all

// ServerRecipeBook.sendInitialRecipeBook: the book's settings, then every recipe (replacing what the client had)
void sendInitialRecipeBook(Packet& packet, Server& server) {
	Player& player = *packet.getPlayer();
	Buffer	settings;
	for (bool setting : player.recipeBookSettings()) settings.writeBool(setting);
	packet.sendPacket(PacketId::Play::Clientbound::RECIPE_BOOK_SETTINGS, settings, server);

	const std::vector<RecipeManager::DisplayInfo>& displays = server.levelOf(*packet.getPlayer()).recipes().displays();
	Buffer										   add;
	add.writeVarInt(static_cast<int32_t>(displays.size()));
	for (const RecipeManager::DisplayInfo& display : displays) {
		add.writeBytes(display.entry);
		add.writeByte(0); // Flags: no notification, no highlight
	}
	add.writeBool(true); // Replace
	packet.sendPacket(PacketId::Play::Clientbound::RECIPE_BOOK_ADD, add, server);
}

// handlePlaceRecipe: a click on a recipe of the book fills the grid, or shows it as a ghost when items are missing
void handlePlaceRecipePacket(Packet& packet, Server& server) {
	Buffer& data		= packet.getData();
	int		containerId = data.readVarInt();
	int		displayId	= data.readVarInt();
	bool	useMaxItems = data.readBool();
	Player& player		= *packet.getPlayer();
	Level&	level		= server.levelOf(*packet.getPlayer());
	Menu&	menu		= Menus::current(player, level);
	if (player.getGameMode() == GameMode::Spectator || menu.containerId() != containerId || !menu.stillValid()) return;
	const RecipeManager::DisplayInfo* display = level.recipes().display(displayId);
	// ----- Furnaces: AbstractFurnaceMenu.handlePlacement, for a recipe of the furnace's type -----
	if (auto* furnace = dynamic_cast<AbstractFurnaceMenu*>(&menu)) {
		if (!display || display->recipe->type != furnace->recipeType() || display->recipe->placement.empty()) return;
		if (furnace->handlePlacement(*display->recipe, useMaxItems, player.getGameMode() == GameMode::Creative)) {
			Buffer ghost;
			ghost.writeVarInt(containerId);
			ghost.writeBytes(display->display);
			packet.sendPacket(PacketId::Play::Clientbound::PLACE_GHOST_RECIPE, ghost, server);
		}
		return;
	}
	// ----- End furnaces -----
	auto*							  crafting = dynamic_cast<CraftingGridMenu*>(&menu);
	if (!display || !crafting || display->recipe->type != RecipeType::Crafting || display->recipe->placement.empty()) return;
	if (crafting->placeRecipe(*display->recipe, useMaxItems)) {
		Buffer ghost;
		ghost.writeVarInt(containerId);
		ghost.writeBytes(display->display);
		packet.sendPacket(PacketId::Play::Clientbound::PLACE_GHOST_RECIPE, ghost, server);
	}
}

// The book opened or closed, filtering or not, for one of its types (crafting, furnace, blast furnace, smoker)
void handleRecipeBookChangeSettingsPacket(Packet& packet, Server& server) {
	(void)server;
	Buffer& data = packet.getData();
	int		type = data.readVarInt();
	bool	open = data.readBool();
	bool	filtering = data.readBool();
	if (type < 0 || type >= 4) return;
	packet.getPlayer()->recipeBookSettings()[type * 2]	   = open;
	packet.getPlayer()->recipeBookSettings()[type * 2 + 1] = filtering;
}