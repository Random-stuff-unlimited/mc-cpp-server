#ifndef SPAWN_EGG_ITEM_HPP
#define SPAWN_EGG_ITEM_HPP

#include "world/BlockPos.hpp"
#include "world/item/ItemStack.hpp"

class Level;
class Mob;
class Player;

// Spawn eggs (vanilla's SpawnEggItem): the item's minecraft:entity_data type, from entity_types.json
namespace SpawnEggItem {
	// SpawnEggItem.useOn: the mob appears in the clicked block if it has no collision, else next to the clicked face
	// (standing on it when clicking a top face). One egg is used, except in creative. player may be null. Returns the
	// mob, nullptr if the item isn't an egg or nothing spawned. Spawners and trial spawners aren't ported: nothing
	// happens on them (vanilla changes their mob)
	Mob* useOn(Level& level, Player* player, ItemStack& stack, const BlockPos& clicked, Direction face);
} // namespace SpawnEggItem

#endif
