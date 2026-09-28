#include "world/inventory/Menu.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/PlayerInventory.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace {
	// EquipmentSlot.getIndex of the armor slots: feet 0 to head 3
	int armorIndex(GameData::EquipmentSlot slot) {
		switch (slot) {
		case GameData::EquipmentSlot::Feet:
			return 0;
		case GameData::EquipmentSlot::Legs:
			return 1;
		case GameData::EquipmentSlot::Chest:
			return 2;
		default:
			return 3;
		}
	}
	bool isHumanoidArmor(GameData::EquipmentSlot slot) {
		return slot == GameData::EquipmentSlot::Head || slot == GameData::EquipmentSlot::Chest || slot == GameData::EquipmentSlot::Legs ||
			   slot == GameData::EquipmentSlot::Feet;
	}
} // namespace

// ----- InventoryMenu -----

InventoryMenu::InventoryMenu(Level& level, Player& player, PlayerInventory& inventory)
	: CraftingGridMenu(level, player, 0, "", 2, 2), _inventory(inventory), _craft(inventory, 1, 4) {
	setGrid(_craft);
	addSlot(_result, 0, Slot::Kind::Result);
	for (int i = 0; i < 4; i++) addSlot(_craft, i);
	// Armor: head (inventory 39) to feet (36)
	const GameData::EquipmentSlot armor[4] = {GameData::EquipmentSlot::Head, GameData::EquipmentSlot::Chest, GameData::EquipmentSlot::Legs,
											  GameData::EquipmentSlot::Feet};
	for (int i = 0; i < 4; i++) addSlot(_inventory, 39 - i, Slot::Kind::Armor).armor = armor[i];
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	addSlot(_inventory, 40);
}

ItemStack InventoryMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack&				stack	 = slot.item();
	ItemStack				original = stack;
	const auto*				item	 = _gameData.getItemProperties(stack.item);
	GameData::EquipmentSlot equip	 = item ? item->equipmentSlot : GameData::EquipmentSlot::MainHand;
	bool					moved;
	if (index == 0) {
		moved = moveItemStackTo(stack, 9, 45, true);
	} else if (index >= 1 && index < 9) {
		moved = moveItemStackTo(stack, 9, 45, false);
	} else if (isHumanoidArmor(equip) && !_slots[8 - armorIndex(equip)].hasItem()) {
		int target = 8 - armorIndex(equip);
		moved	   = moveItemStackTo(stack, target, target + 1, false);
	} else if (equip == GameData::EquipmentSlot::OffHand && !_slots[45].hasItem()) {
		moved = moveItemStackTo(stack, 45, 46, false);
	} else if (index >= 9 && index < 36) {
		moved = moveItemStackTo(stack, 36, 45, false);
	} else if (index >= 36 && index < 45) {
		moved = moveItemStackTo(stack, 9, 36, false);
	} else {
		moved = moveItemStackTo(stack, 9, 45, false);
	}
	if (!moved) return {};
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	if (stack.count == original.count) return {};
	ItemStack left = stack; // The result slot gets the next craft in onTake
	onTake(slot, left);
	if (index == 0 && !left.isEmpty()) _level.dropFromPlayer(_player, std::move(left), false);
	return original;
}

void InventoryMenu::removed() {
	Menu::removed();
	_result.removeItemNoUpdate(0);
	clearContainer(_craft);
}
// ----- CraftingGridMenu -----

CraftingGridMenu::CraftingGridMenu(Level& level, Player& player, int containerId, std::string type, int width, int height)
	: Menu(level, player, containerId, std::move(type)), _gridWidth(width), _gridHeight(height) {}

void CraftingGridMenu::setGrid(Container& grid) {
	_grid = &grid;
	auto changed = [this] {
		if (!_placingRecipe) updateCraftingResult(*_grid, _gridWidth, _result);
	};
	if (auto* simple = dynamic_cast<SimpleContainer*>(&grid)) simple->changed = changed;
	if (auto* range = dynamic_cast<InventoryRange*>(&grid)) range->changed = changed;
}

void CraftingGridMenu::onTake(Slot& slot, const ItemStack& stack) {
	if (slot.kind == Slot::Kind::Result) {
		takeCraftingResult(*_grid, _gridWidth);
	} else {
		Menu::onTake(slot, stack);
	}
}

namespace {
	// PlaceRecipeHelper.placeRecipe: the recipe's cells over the grid, a smaller shaped recipe centered when it
	// fits twice; place(ingredient index, grid slot) for each cell
	template <typename Place>
	void placeInGrid(int gridWidth, int gridHeight, int width, int height, const std::vector<int>& cells, Place place) {
		size_t next = 0;
		int	   slot = 0;
		for (int y = 0; y < gridHeight; y++) {
			bool centerY = height < gridHeight / 2.0F;
			int	 offsetY = static_cast<int>(std::floor(gridHeight / 2.0F - height / 2.0F));
			if (centerY && offsetY > y) {
				slot += gridWidth;
				y++;
			}
			for (int x = 0; x < gridWidth; x++) {
				if (next >= cells.size()) return;
				bool centerX = width < gridWidth / 2.0F;
				int	 offsetX = static_cast<int>(std::floor(gridWidth / 2.0F - width / 2.0F));
				int	 end	 = width;
				bool inside	 = x < width;
				if (centerX) {
					end	   = offsetX + width;
					inside = offsetX <= x && x < offsetX + width;
				}
				if (inside) {
					place(cells[next++], slot);
				} else if (end == x) {
					slot += gridWidth - x;
					break;
				}
				slot++;
			}
		}
	}
} // namespace

bool CraftingGridMenu::placeRecipe(const Recipe& recipe, bool useMaxItems) {
	bool creative = _player.getGameMode() == GameMode::Creative;
	if (!creative && !testClearGrid()) return false;
	_placingRecipe = true;
	StackedItemContents contents;
	PlayerContainer		inventory(_player.inventory());
	for (int i = 0; i < 36; i++) contents.accountSimpleStack(inventory.item(i), _gameData);
	for (int i = 0; i < _grid->size(); i++) contents.accountSimpleStack(_grid->item(i), _gameData);

	bool ghost = false;
	if (!contents.canCraft(recipe.placement)) {
		clearGrid();
		ghost = true;
	} else {
		// ServerPlaceRecipe.placeRecipe: once more than what the grid holds for this recipe (or all at once)
		std::vector<ItemStack> items(static_cast<size_t>(_grid->size()));
		for (int i = 0; i < _grid->size(); i++) items[i] = _grid->item(i);
		bool matches = recipe.matches(CraftingInput::ofPositioned(_gridWidth, _gridHeight, items).input);
		int	 biggest = contents.biggestCraftableStack(recipe.placement);
		bool full	 = false;
		if (matches) {
			for (int i = 0; i < _grid->size(); i++) {
				const ItemStack& stack = _grid->item(i);
				if (!stack.isEmpty() && std::min(biggest, maxStackSize(stack)) < stack.count + 1) full = true;
			}
		}
		int amount = 1;
		if (useMaxItems) {
			amount = biggest;
		} else if (matches) {
			amount = INT32_MAX;
			for (const ItemStack& stack : items) {
				if (!stack.isEmpty()) amount = std::min(amount, stack.count);
			}
			if (amount != INT32_MAX) amount++;
		}
		std::vector<int> picked;
		if (!full && contents.canCraft(recipe.placement, amount, &picked)) {
			// clampToMaxStackSize
			int clamped = amount;
			for (int item : picked) {
				const GameData::ItemProperties* properties = _gameData.getItemProperties(item);
				clamped = std::min(clamped, properties ? properties->maxStackSize : 64);
			}
			bool ok = clamped == amount || contents.canCraft(recipe.placement, clamped, &picked);
			if (ok) {
				clearGrid();
				int width  = recipe.kind == Recipe::Kind::Shaped ? recipe.width : _gridWidth;
				int height = recipe.kind == Recipe::Kind::Shaped ? recipe.height : _gridHeight;
				placeInGrid(_gridWidth, _gridHeight, width, height, recipe.slotsToIngredient, [&](int ingredient, int gridSlot) {
					if (ingredient < 0) return;
					Slot& slot = _slots[1 + gridSlot];
					for (int left = clamped; left > 0;) {
						left = moveItemToGrid(slot, picked[ingredient], left);
						if (left < 0) return;
					}
				});
			}
		}
	}
	_placingRecipe = false;
	updateCraftingResult(*_grid, _gridWidth, _result, &recipe); // finishPlacingRecipe
	return ghost;
}

// clearGrid: the grid's items back into the inventory (placeItemBackInInventory), the rest dropped
void CraftingGridMenu::clearGrid() {
	for (int i = 0; i < _grid->size(); i++) {
		ItemStack stack = _grid->removeItemNoUpdate(i);
		if (stack.isEmpty()) continue;
		_player.inventory().add(stack, _player.getSelectedSlot(), false, _gameData);
		if (!stack.isEmpty()) _level.dropFromPlayer(_player, std::move(stack), false);
	}
	_result.removeItemNoUpdate(0);
	_grid->setChanged();
}

// testClearGrid: whether the grid's items would all fit back into the inventory
bool CraftingGridMenu::testClearGrid() {
	PlayerContainer inventory(_player.inventory());
	int				free = 0;
	for (int i = 0; i < 36; i++) {
		if (inventory.item(i).isEmpty()) free++;
	}
	// getSlotWithRemainingSpace: the selected slot, the offhand, then the inventory
	auto roomFor = [&](const ItemStack& stack) {
		auto room = [&](const ItemStack& slot) {
			return !slot.isEmpty() && slot.sameItemSameComponents(stack) && maxStackSize(slot) > 1 && slot.count < maxStackSize(slot);
		};
		if (room(inventory.item(_player.getSelectedSlot())) || room(inventory.item(40))) return true;
		for (int i = 0; i < 36; i++) {
			if (room(inventory.item(i))) return true;
		}
		return false;
	};
	std::vector<ItemStack> extra;
	for (int i = 0; i < _grid->size(); i++) {
		ItemStack stack = _grid->item(i);
		if (stack.isEmpty()) continue;
		bool room = roomFor(stack);
		if (!room && static_cast<int>(extra.size()) <= free) {
			for (ItemStack& other : extra) {
				int max = maxStackSize(other);
				if (other.item == stack.item && other.count != max && other.count + stack.count <= max) {
					other.grow(stack.count);
					stack.count = 0;
					break;
				}
			}
			if (stack.count > 0) {
				if (static_cast<int>(extra.size()) >= free) return false;
				extra.push_back(stack);
			}
		} else if (!room) {
			return false;
		}
	}
	return true;
}

int CraftingGridMenu::moveItemToGrid(Slot& slot, int item, int count) {
	PlayerContainer	 inventory(_player.inventory());
	ItemStack&		 current = slot.item();
	// Inventory.findSlotMatchingCraftingIngredient: the item, usable for crafting, the same as the slot's
	int found = -1;
	for (int i = 0; i < 36 && found < 0; i++) {
		const ItemStack& stack = inventory.item(i);
		if (stack.isEmpty() || stack.item != item) continue;
		StackedItemContents usable;
		usable.accountSimpleStack(stack, _gameData);
		Ingredient only;
		only.items = {item};
		if (!usable.canCraft({only}) || (!current.isEmpty() && !current.sameItemSameComponents(stack))) continue;
		found = i;
	}
	if (found < 0) return -1;
	ItemStack taken = count < inventory.item(found).count ? inventory.removeItem(found, count) : inventory.removeItemNoUpdate(found);
	int		  moved = taken.count;
	if (current.isEmpty()) {
		slot.set(std::move(taken));
	} else {
		current.grow(moved);
	}
	return count - moved;
}
// ----- CraftingMenu -----

CraftingMenu::CraftingMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos)
	: CraftingGridMenu(level, player, containerId, "minecraft:crafting", 3, 3), _inventory(inventory), _pos(pos),
	  _table(level.gameData().getStaticId("minecraft:block", "minecraft:crafting_table")) {
	setGrid(_craft);
	addSlot(_result, 0, Slot::Kind::Result);
	for (int i = 0; i < 9; i++) addSlot(_craft, i);
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
}

bool CraftingMenu::stillValid() {
	if (_gameData.getBlockOfState(_level.getBlockState(_pos)) != _table) return false;
	// Player.canInteractWithBlock(pos, 4.0): the block's box within the interaction range + 4
	double range = (_player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double eyeY	 = _player.getY() + 1.62;
	double dx	 = std::max({_pos.x - _player.getX(), 0.0, _player.getX() - (_pos.x + 1.0)});
	double dy	 = std::max({_pos.y - eyeY, 0.0, eyeY - (_pos.y + 1.0)});
	double dz	 = std::max({_pos.z - _player.getZ(), 0.0, _player.getZ() - (_pos.z + 1.0)});
	return dx * dx + dy * dy + dz * dz < range * range;
}

// Result: into the inventory, hotbar first. Inventory: into the grid, else between the inventory and the hotbar.
// Grid: into the inventory
ItemStack CraftingMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index == 0) {
		if (!moveItemStackTo(stack, 10, 46, true)) return {};
	} else if (index >= 10 && index < 46) {
		if (!moveItemStackTo(stack, 1, 10, false)) {
			if (index < 37) {
				if (!moveItemStackTo(stack, 37, 46, false)) return {};
			} else if (!moveItemStackTo(stack, 10, 37, false)) {
				return {};
			}
		}
	} else if (!moveItemStackTo(stack, 10, 46, false)) {
		return {};
	}
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	if (stack.count == original.count) return {};
	ItemStack left = stack; // The result slot gets the next craft in onTake
	onTake(slot, left);
	if (index == 0 && !left.isEmpty()) _level.dropFromPlayer(_player, std::move(left), false);
	return original;
}



void CraftingMenu::removed() {
	Menu::removed();
	clearContainer(_craft);
}
