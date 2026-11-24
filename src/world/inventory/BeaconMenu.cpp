#include "world/inventory/BeaconMenu.hpp"

#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"

#include <algorithm>

BeaconMenu::BeaconMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos)
	: Menu(level, player, containerId, "minecraft:beacon"), _inventory(inventory), _pos(pos) {
	// PaymentSlot: one payment item at a time
	Slot& payment	   = addSlot(_payment, 0);
	payment.placeRule  = [this](const ItemStack& stack) { return isPayment(stack); };
	payment.stackLimit = [](const ItemStack&) { return 1; };
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	addDataSlots(3);
}

bool BeaconMenu::isPayment(const ItemStack& stack) const {
	return !stack.isEmpty() && _gameData.isInTag("minecraft:item", "minecraft:beacon_payment_items", stack.item);
}

BeaconBlockEntity* BeaconMenu::beacon() const { return dynamic_cast<BeaconBlockEntity*>(_level.getBlockEntity(_pos)); }

// The levels, then each power as its registry id + 1 (0: none)
int BeaconMenu::dataSlot(int index) const {
	BeaconBlockEntity* entity = beacon();
	if (!entity) return 0;
	if (index == 0) return entity->levels();
	BeaconBlockEntity::Power power = index == 1 ? entity->primary() : entity->secondary();
	return power ? static_cast<int>(*power) + 1 : 0;
}

bool BeaconMenu::updateEffects(BeaconBlockEntity::Power primary, BeaconBlockEntity::Power secondary) {
	BeaconBlockEntity* entity = beacon();
	if (!entity || _payment.item(0).isEmpty()) return false;
	entity->setPowers(primary, secondary);
	_payment.removeItem(0, 1);
	return true;
}

bool BeaconMenu::stillValid() {
	if (!beacon()) return false;
	// Player.canInteractWithBlock(pos, 4.0)
	double range = (_player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double dx	 = std::max({_pos.x - _player.getX(), 0.0, _player.getX() - (_pos.x + 1.0)});
	double dy	 = std::max({_pos.y - (_player.getY() + 1.62), 0.0, (_player.getY() + 1.62) - (_pos.y + 1.0)});
	double dz	 = std::max({_pos.z - _player.getZ(), 0.0, _player.getZ() - (_pos.z + 1.0)});
	return dx * dx + dy * dy + dz * dz < range * range;
}

ItemStack BeaconMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index == 0) {
		if (!moveItemStackTo(stack, 1, 37, true)) return {};
	} else if (isPayment(stack) && _payment.item(0).isEmpty()) {
		ItemStack one = stack.copyWithCount(1);
		stack.shrink(1);
		_payment.setItem(0, std::move(one));
	} else if (index >= 1 && index < 28) {
		if (!moveItemStackTo(stack, 28, 37, false)) return {};
	} else if (!moveItemStackTo(stack, 1, 28, false)) {
		return {};
	}
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	if (stack.count == original.count) return {};
	onTake(slot, stack);
	return original;
}

void BeaconMenu::removed() {
	Menu::removed();
	clearContainer(_payment);
}

namespace Menus {
	void openBeacon(Player& player, Level& level, const BlockPos& pos) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int	   id = player.nextContainerCounter();
		Buffer title;
		TextComponent::writeTranslatable(title, "container.beacon", {});
		open(player, level, std::make_unique<BeaconMenu>(level, player, player.inventory(), id, pos), title.getData());
	}
} // namespace Menus
