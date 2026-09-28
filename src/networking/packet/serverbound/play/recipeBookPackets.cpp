#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/inventory/Menu.hpp"

// The recipe book. Recipes aren't unlocked by advancements yet (there are none): every player knows them all

// ServerRecipeBook.sendInitialRecipeBook: the book's settings, then every recipe (replacing what the client had)
void sendInitialRecipeBook(Packet& packet, Server& server) {
	Player& player = *packet.getPlayer();
	Buffer	settings;
	for (bool setting : player.recipeBookSettings()) settings.writeBool(setting);
	packet.sendPacket(PacketId::Play::Clientbound::RECIPE_BOOK_SETTINGS, settings, server);

	const std::vector<RecipeManager::DisplayInfo>& displays = server.getLevel().recipes().displays();
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
	Level&	level		= server.getLevel();
	Menu&	menu		= Menus::current(player, level);
	if (player.getGameMode() == GameMode::Spectator || menu.containerId() != containerId || !menu.stillValid()) return;
	const RecipeManager::DisplayInfo* display = level.recipes().display(displayId);
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
