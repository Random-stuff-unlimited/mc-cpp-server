#include "world/inventory/Menu.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/PlayerInventory.hpp"

// ----- ContainerMenu -----

ContainerMenu::ContainerMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, std::string type,
							 std::shared_ptr<Container> container, Slot::Kind kind)
	: Menu(level, player, containerId, std::move(type)), _inventory(inventory), _container(std::move(container)) {
	_container->startOpen(player);
	for (int i = 0; i < _container->size(); i++) addSlot(*_container, i, kind);
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
}

ItemStack ContainerMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	int		   size		= _container->size();
	bool	   moved	= index < size ? moveItemStackTo(stack, size, static_cast<int>(_slots.size()), true) : moveItemStackTo(stack, 0, size, false);
	if (!moved) return {};
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	return original;
}

void ContainerMenu::removed() {
	Menu::removed();
	_container->stopOpen(_player);
}
