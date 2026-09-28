#include "world/item/PlayerInventory.hpp"

#include "data/GameData.hpp"

#include <algorithm>

namespace {
	int maxStackSize(const ItemStack& stack, const GameData& gameData) {
		const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
		return item ? item->maxStackSize : 64;
	}
} // namespace

int PlayerInventory::windowSlot(int inventoryIndex) {
	if (inventoryIndex < 9) return HOTBAR + inventoryIndex;
	if (inventoryIndex < 36) return inventoryIndex;
	if (inventoryIndex < 40) return 8 - (inventoryIndex - 36); // Feet (36) is window slot 8, head (39) slot 5
	return OFFHAND;
}

ItemStack& PlayerInventory::getMutable(int slot) {
	_changed.push_back(slot);
	return _slots.at(slot);
}

void PlayerInventory::set(int slot, ItemStack stack) {
	_slots.at(slot) = std::move(stack);
	_changed.push_back(slot);
}

void PlayerInventory::setFromClient(int slot, ItemStack stack) { _slots.at(slot) = std::move(stack); }

std::vector<int> PlayerInventory::takeChanged() {
	std::vector<int> changed;
	changed.swap(_changed);
	std::sort(changed.begin(), changed.end());
	changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
	return changed;
}

bool PlayerInventory::hasRemainingSpace(const ItemStack& slot, const ItemStack& stack, const GameData& gameData) const {
	return !slot.isEmpty() && slot.sameItemSameComponents(stack) && maxStackSize(slot, gameData) > 1 && slot.count < maxStackSize(slot, gameData);
}

int PlayerInventory::addResource(int window, const ItemStack& stack, const GameData& gameData) {
	int		   left	  = stack.count;
	ItemStack& target = _slots[window];
	if (target.isEmpty()) target = stack.copyWithCount(0);
	int moved = std::min(left, maxStackSize(target, gameData) - target.count);
	if (moved == 0) return left;
	target.grow(moved);
	_changed.push_back(window);
	return left - moved;
}

bool PlayerInventory::add(ItemStack& stack, int selectedHotbarSlot, bool infiniteMaterials, const GameData& gameData) {
	if (stack.isEmpty()) return false;
	int before;
	do {
		before = stack.count;
		// getSlotWithRemainingSpace, then getFreeSlot
		int window = -1;
		if (hasRemainingSpace(_slots[windowSlot(selectedHotbarSlot)], stack, gameData)) {
			window = windowSlot(selectedHotbarSlot);
		} else if (hasRemainingSpace(_slots[OFFHAND], stack, gameData)) {
			window = OFFHAND;
		} else {
			for (int i = 0; i < 36 && window < 0; i++) {
				if (hasRemainingSpace(_slots[windowSlot(i)], stack, gameData)) window = windowSlot(i);
			}
		}
		for (int i = 0; i < 36 && window < 0; i++) {
			if (_slots[windowSlot(i)].isEmpty()) window = windowSlot(i);
		}
		if (window >= 0) stack.count = addResource(window, stack, gameData);
	} while (!stack.isEmpty() && stack.count < before);

	// Like vanilla, compared to the start of the last pass: when a pass adds nothing after an earlier one added some,
	// this returns false although the stack shrank (a partial pickup takes items without the pickup animation)
	if (stack.count == before && infiniteMaterials) {
		stack.count = 0;
		return true;
	}
	return stack.count < before;
}
