#ifndef POTION_BREWING_HPP
#define POTION_BREWING_HPP

#include "world/item/ItemStack.hpp"

#include <vector>

class GameData;

// Vanilla's PotionBrewing with its vanilla mixes (PotionBrewing.addVanillaMixes): which ingredient turns a potion into
// another (potion mixes), or a potion item into another kind of bottle (container mixes: splash, lingering). Potions
// are minecraft:potion registry ids, kept in the minecraft:potion_contents component
class PotionBrewing {
  public:
	explicit PotionBrewing(const GameData& gameData);

	bool isIngredient(const ItemStack& stack) const { return isContainerIngredient(stack) || isPotionIngredient(stack); }
	bool isContainerIngredient(const ItemStack& stack) const;
	bool isPotionIngredient(const ItemStack& stack) const;
	bool isBrewablePotion(int potion) const;
	// hasMix: the ingredient does something to this bottle
	bool hasMix(const ItemStack& bottle, const ItemStack& ingredient) const;
	bool hasContainerMix(const ItemStack& bottle, const ItemStack& ingredient) const;
	bool hasPotionMix(const ItemStack& bottle, const ItemStack& ingredient) const;
	// mix: what the bottle becomes with the ingredient (itself if nothing)
	ItemStack mix(const ItemStack& ingredient, const ItemStack& bottle) const;

	// The potion of a stack (PotionContents.potion), -1 if none
	int		  potionOf(const ItemStack& stack) const;
	// PotionContents.createItemStack: one item holding this potion
	ItemStack createItemStack(int item, int potion) const;

  private:
	// Mix<T>: from, ingredient (one item: Ingredient.of), to
	struct Mix {
		int from, ingredient, to;
	};
	const GameData&	 _gameData;
	std::vector<int> _containers; // Items
	std::vector<Mix> _potionMixes;
	std::vector<Mix> _containerMixes;
};

#endif
