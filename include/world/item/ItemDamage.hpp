#ifndef ITEM_DAMAGE_HPP
#define ITEM_DAMAGE_HPP

#include "world/item/ItemStack.hpp"

class Level;
class Player;
class GameData;

// Durability (vanilla's ItemStack.hurtAndBreak, getDamageValue, getMaxDamage): tools, weapons and armor wear out
namespace ItemDamage {
	// max_damage of the item, 0 if it can't be damaged (or unbreakable)
	int	 maxDamage(const GameData& gameData, const ItemStack& stack);
	int	 damage(const GameData& gameData, const ItemStack& stack);
	void setDamage(const GameData& gameData, ItemStack& stack, int damage);
	// ItemStack.hurtAndBreak: `amount` durability lost (less with unbreaking), broken at max_damage (the break sound,
	// the entity event). player may be null (a mob's item); slot: the player's inventory slot, for the event
	void hurtAndBreak(Level& level, ItemStack& stack, int amount, Player* player, int slot);
} // namespace ItemDamage

#endif
