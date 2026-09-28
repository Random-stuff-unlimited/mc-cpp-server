#include "world/inventory/StorageMenus.hpp"

#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/Recipes.hpp"

// ===== LecternMenu =====

LecternMenu::LecternMenu(Level& level, Player& player, int containerId, std::shared_ptr<BlockEntity> lectern)
	: Menu(level, player, containerId, "minecraft:lectern"), _keepAlive(std::move(lectern)), _lectern(dynamic_cast<LecternBlockEntity*>(_keepAlive.get())) {
	addSlot(_lectern->bookAccess(), 0);
	addDataSlots(1);
}

void LecternMenu::setPage(int page) {
	_lectern->setPage(page);
	broadcastChanges();
}

bool LecternMenu::clickMenuButton(int button) {
	if (button >= 100) {
		setPage(button - 100);
		return true;
	}
	switch (button) {
	case 1:
		setPage(_lectern->page() - 1);
		return true;
	case 2:
		setPage(_lectern->page() + 1);
		return true;
	case 3: {
		// Player.mayBuild: not in adventure or spectator mode
		if (_player.getGameMode() == GameMode::Adventure || _player.getGameMode() == GameMode::Spectator) return false;
		ItemStack book = _lectern->bookAccess().takeBook();
		_lectern->bookAccess().setChanged();
		if (!_player.inventory().add(book, _player.getSelectedSlot(), _player.getGameMode() == GameMode::Creative, _gameData)) {
			_level.dropFromPlayer(_player, std::move(book), false);
		}
		return true;
	}
	default:
		return false;
	}
}

// ===== CrafterMenu =====

CrafterMenu::CrafterMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, std::shared_ptr<BlockEntity> crafter)
	: Menu(level, player, containerId, "minecraft:crafter_3x3"), _inventory(inventory), _keepAlive(std::move(crafter)),
	  _crafter(dynamic_cast<CrafterBlockEntity*>(_keepAlive.get())) {
	_crafter->startOpen(player);
	for (int i = 0; i < 9; i++) addSlot(*_crafter, i, Slot::Kind::Crafter);
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	addSlot(_result, 0, Slot::Kind::NonInteractive);
	addDataSlots(10);
	refreshRecipeResult();
	for (Slot& slot : _slots) _lastSlots.push_back(slot.item());
}

void CrafterMenu::refreshRecipeResult() {
	std::vector<ItemStack> items(9);
	for (int i = 0; i < 9; i++) items[i] = _crafter->item(i);
	CraftingInput input	 = CraftingInput::ofPositioned(3, 3, items).input;
	const Recipe* recipe = _level.recipes().getRecipeFor(RecipeType::Crafting, input);
	_result.item(0)		 = recipe ? recipe->assemble(input) : ItemStack();
}

void CrafterMenu::beforeBroadcastChanges() {
	bool changed = false;
	for (size_t i = 0; i < _slots.size() && i < _lastSlots.size(); i++) {
		const ItemStack& now = _slots[i].item();
		if (now.item == _lastSlots[i].item && now.count == _lastSlots[i].count && now.components == _lastSlots[i].components) continue;
		_lastSlots[i] = now;
		changed		  = true;
	}
	if (changed) refreshRecipeResult();
	// The result slot itself is looked at with the others: keep it known
	if (!_lastSlots.empty()) _lastSlots.back() = _result.item(0);
}

// Grid: into the inventory (hotbar end first). Inventory: into the grid
ItemStack CrafterMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index < 9) {
		if (!moveItemStackTo(stack, 9, 45, true)) return {};
	} else if (!moveItemStackTo(stack, 0, 9, false)) {
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

namespace Menus {
	void openStorage(Player& player, Level& level, const std::function<std::unique_ptr<Menu>(int)>& create, const std::vector<uint8_t>& customName,
					 const std::string& defaultName) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int					 id	   = player.nextContainerCounter();
		std::vector<uint8_t> title = customName;
		if (title.empty()) {
			Buffer name;
			TextComponent::writeTranslatable(name, defaultName, {});
			title = name.getData();
		}
		open(player, level, create(id), title);
	}
} // namespace Menus
