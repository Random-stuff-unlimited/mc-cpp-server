#ifndef FUEL_VALUES_HPP
#define FUEL_VALUES_HPP

#include "world/item/ItemStack.hpp"

#include <vector>

class GameData;

// Vanilla's FuelValues: how long each item burns in a furnace, in ticks (FuelValues.vanillaBurnTimes). Item tags
// are resolved through the game data's tags
class FuelValues {
  public:
	// vanillaBurnTimes(standard): standard is the burn time of the smelting of one item (200)
	explicit FuelValues(const GameData& gameData, int standard = 200);

	bool isFuel(const ItemStack& stack) const { return !stack.isEmpty() && burnDuration(stack) > 0; }
	int	 burnDuration(const ItemStack& stack) const {
		 return stack.isEmpty() || stack.item < 0 || stack.item >= static_cast<int>(_byItem.size()) ? 0 : _byItem[stack.item];
	}

  private:
	std::vector<int> _byItem; // By minecraft:item id, 0 for an item that isn't a fuel
};

#endif
