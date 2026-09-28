#ifndef PLAYER_INVENTORY_HPP
#define PLAYER_INVENTORY_HPP

#include "world/item/ItemStack.hpp"

#include <array>
#include <vector>

class GameData;

// The player's inventory, by slot of the inventory window (what the protocol uses): 0 crafting result, 1-4 crafting
// grid, 5-8 armor (head to feet), 9-35 main, 36-44 hotbar, 45 offhand.
// Changes made by the server are sent to the client at the end of the tick (Set Container Slot)
class PlayerInventory {
  public:
	static constexpr int SIZE	 = 46;
	static constexpr int HOTBAR	 = 36;
	static constexpr int OFFHAND = 45;

	const ItemStack& get(int slot) const { return _slots.at(slot); }
	ItemStack&		 getMutable(int slot);
	// A change by the server: sent to the client
	void set(int slot, ItemStack stack);
	// A change the client already knows (creative inventory)
	void setFromClient(int slot, ItemStack stack);

	// Inventory.add: into stacks of the same item first (selected hotbar slot, offhand, then hotbar and main), then
	// into the first empty slot. The stack is shrunk by what fit. Returns false if nothing fit. With infinite
	// materials (creative), what doesn't fit disappears
	bool add(ItemStack& stack, int selectedHotbarSlot, bool infiniteMaterials, const GameData& gameData);

	// Slots changed by the server since the last call, and the state id of each Set Container Slot
	std::vector<int> takeChanged();
	int				 nextStateId() { return ++_stateId; }

  private:
	std::array<ItemStack, SIZE> _slots;
	std::vector<int>			_changed;
	int							_stateId = 0;

	// Vanilla Inventory index (0-8 hotbar, 9-35 main, 36-39 feet to head, 40 offhand) to window slot
	static int windowSlot(int inventoryIndex);
	bool	   hasRemainingSpace(const ItemStack& slot, const ItemStack& stack, const GameData& gameData) const;
	// Inventory.addResource: returns what didn't fit
	int addResource(int window, const ItemStack& stack, const GameData& gameData);
};

#endif
