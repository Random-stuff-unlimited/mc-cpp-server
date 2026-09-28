#ifndef PLAYER_INVENTORY_HPP
#define PLAYER_INVENTORY_HPP

#include "world/item/ItemStack.hpp"

#include <array>
#include <vector>

class GameData;

// The player's inventory, by slot of the inventory window (what the protocol uses): 0 crafting result, 1-4 crafting
// grid, 5-8 armor (head to feet), 9-35 main, 36-44 hotbar, 45 offhand.
// The player's menu sends the client what changed, once per tick (see Menu)
class PlayerInventory {
  public:
	static constexpr int SIZE	 = 46;
	static constexpr int HOTBAR	 = 36;
	static constexpr int OFFHAND = 45;

	const ItemStack& get(int slot) const { return _slots.at(slot); }
	ItemStack&		 getMutable(int slot);
	void set(int slot, ItemStack stack);

	// Inventory.add: into stacks of the same item first (selected hotbar slot, offhand, then hotbar and main), then
	// into the first empty slot. The stack is shrunk by what fit. Returns false if nothing fit. With infinite
	// materials (creative), what doesn't fit disappears
	bool add(ItemStack& stack, int selectedHotbarSlot, bool infiniteMaterials, const GameData& gameData);

	// Vanilla Inventory index (0-8 hotbar, 9-35 main, 36-39 feet to head, 40 offhand) to window slot
	static int windowSlot(int inventoryIndex);

  private:
	std::array<ItemStack, SIZE> _slots;
	bool	   hasRemainingSpace(const ItemStack& slot, const ItemStack& stack, const GameData& gameData) const;
	// Inventory.addResource: returns what didn't fit
	int addResource(int window, const ItemStack& stack, const GameData& gameData);
};

#endif
