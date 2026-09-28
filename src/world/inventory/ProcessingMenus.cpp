#include "world/inventory/ProcessingMenus.hpp"

#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/FuelValues.hpp"
#include "world/item/PlayerInventory.hpp"
#include "world/item/PotionBrewing.hpp"

#include <algorithm>
#include <climits>

namespace {
	int itemMaxStackSize(const GameData& gameData, const ItemStack& stack) {
		const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
		return item ? item->maxStackSize : 64;
	}
	bool isItem(const GameData& gameData, const ItemStack& stack, const char* name) {
		return !stack.isEmpty() && stack.item == gameData.getStaticId("minecraft:item", name);
	}
} // namespace

// ===== AbstractFurnaceMenu =====

AbstractFurnaceMenu::AbstractFurnaceMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId,
										 std::shared_ptr<AbstractFurnaceBlockEntity> furnace)
	: Menu(level, player, containerId, furnace->menuType()), _inventory(inventory), _furnace(std::move(furnace)) {
	addSlot(*_furnace, AbstractFurnaceBlockEntity::SLOT_INPUT);
	// FurnaceFuelSlot: fuels and buckets, one bucket at a time
	Slot& fuel		= addSlot(*_furnace, AbstractFurnaceBlockEntity::SLOT_FUEL);
	fuel.placeRule	= [this](const ItemStack& stack) { return isFuel(stack) || isItem(_gameData, stack, "minecraft:bucket"); };
	fuel.stackLimit = [this](const ItemStack& stack) { return isItem(_gameData, stack, "minecraft:bucket") ? 1 : 0; };
	// FurnaceResultSlot: taken only
	addSlot(*_furnace, AbstractFurnaceBlockEntity::SLOT_RESULT).placeRule = [](const ItemStack&) { return false; };
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	addDataSlots(AbstractFurnaceBlockEntity::NUM_DATA_VALUES);
}

int AbstractFurnaceMenu::recipeBookType() const {
	switch (_furnace->recipeType()) {
	case RecipeType::Blasting:
		return 2;
	case RecipeType::Smoking:
		return 3;
	default:
		return 1;
	}
}

// acceptedInputs (the recipe property set of the furnace's inputs): an item some recipe of its type cooks
bool AbstractFurnaceMenu::canSmelt(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.recipes().getRecipeFor(_furnace->recipeType(), CraftingInput::ofPositioned(1, 1, {stack}).input) != nullptr;
}

bool AbstractFurnaceMenu::isFuel(const ItemStack& stack) const { return _level.fuelValues().isFuel(stack); }

// Result: into the inventory, hotbar first. Inventory: a smeltable item into the input, a fuel into the fuel slot,
// else between the inventory and the hotbar. Input and fuel: into the inventory
ItemStack AbstractFurnaceMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index == 2) {
		if (!moveItemStackTo(stack, 3, 39, true)) return {};
		// onQuickCraft: FurnaceResultSlot.checkTakeAchievements
		_furnace->awardUsedRecipesAndPopExperience(_player);
	} else if (index != 1 && index != 0) {
		if (canSmelt(stack)) {
			if (!moveItemStackTo(stack, 0, 1, false)) return {};
		} else if (isFuel(stack)) {
			if (!moveItemStackTo(stack, 1, 2, false)) return {};
		} else if (index >= 3 && index < 30) {
			if (!moveItemStackTo(stack, 30, 39, false)) return {};
		} else if (index >= 30 && index < 39 && !moveItemStackTo(stack, 3, 30, false)) {
			return {};
		}
	} else if (!moveItemStackTo(stack, 3, 39, false)) {
		return {};
	}
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	if (stack.count == original.count) return {};
	ItemStack left = stack;
	onTake(slot, left);
	return original;
}

// FurnaceResultSlot.onTake: checkTakeAchievements
void AbstractFurnaceMenu::onTake(Slot& slot, const ItemStack& stack) {
	if (slot.index == 2) _furnace->awardUsedRecipesAndPopExperience(_player);
	Menu::onTake(slot, stack);
}

// handlePlacement: ServerPlaceRecipe.placeRecipe with the input slot as a 1x1 grid; the input and result slots are
// cleared (back into the inventory) first
bool AbstractFurnaceMenu::handlePlacement(const Recipe& recipe, bool useMaxItems, bool creative) {
	if (!creative && !testClearGrid()) return false;
	// Inventory.fillStackedContents, then fillCraftSlotsStackedContents (the furnace's three slots)
	StackedItemContents contents;
	for (int i = 0; i < 36; i++) contents.accountSimpleStack(_inventory.item(i), _gameData);
	for (int i = 0; i < 3; i++) contents.accountStack(_furnace->item(i), itemMaxStackSize(_gameData, _furnace->item(i)));

	// tryPlaceRecipe
	if (!contents.canCraft(recipe.placement)) {
		clearGrid();
		return true;
	}
	// placeRecipe
	const ItemStack& current = _furnace->item(AbstractFurnaceBlockEntity::SLOT_INPUT);
	bool			 matches = recipe.matches(CraftingInput::ofPositioned(1, 1, {current}).input);
	int				 biggest = contents.biggestCraftableStack(recipe.placement);
	if (matches && !current.isEmpty() && std::min(biggest, itemMaxStackSize(_gameData, current)) < current.count + 1) return false;
	// calculateAmountToCraft
	int amount = 1;
	if (useMaxItems) {
		amount = biggest;
	} else if (matches) {
		amount = current.isEmpty() ? INT_MAX : current.count + 1;
	}
	std::vector<int> picked;
	if (!contents.canCraft(recipe.placement, amount, &picked)) return false;
	// clampToMaxStackSize
	int clamped = amount;
	for (int item : picked) {
		const GameData::ItemProperties* properties = _gameData.getItemProperties(item);
		clamped									   = std::min(clamped, properties ? properties->maxStackSize : 64);
	}
	if (clamped != amount) {
		picked.clear();
		if (!contents.canCraft(recipe.placement, clamped, &picked)) return false;
	}
	clearGrid();
	// PlaceRecipeHelper.placeRecipe over a 1x1 grid: the one ingredient into the input slot
	if (!recipe.slotsToIngredient.empty() && recipe.slotsToIngredient[0] >= 0) {
		Slot& input = _slots[AbstractFurnaceBlockEntity::SLOT_INPUT];
		for (int left = clamped; left > 0;) {
			left = moveItemToGrid(input, picked[recipe.slotsToIngredient[0]], left);
			if (left < 0) break;
		}
	}
	return false;
}

// clearGrid: the input and result slots back into the inventory (placeItemBackInInventory), the rest dropped
void AbstractFurnaceMenu::clearGrid() {
	for (int index : {AbstractFurnaceBlockEntity::SLOT_INPUT, AbstractFurnaceBlockEntity::SLOT_RESULT}) {
		ItemStack stack = _slots[index].item();
		if (!stack.isEmpty()) {
			_player.inventory().add(stack, _player.getSelectedSlot(), false, _gameData);
			if (!stack.isEmpty()) _level.dropFromPlayer(_player, std::move(stack), false);
		}
		_slots[index].set(ItemStack());
	}
}

// testClearGrid: whether the input slot's items would fit back into the inventory
bool AbstractFurnaceMenu::testClearGrid() {
	const ItemStack& stack = _furnace->item(AbstractFurnaceBlockEntity::SLOT_INPUT);
	if (stack.isEmpty()) return true;
	// getSlotWithRemainingSpace: the selected slot, the offhand, then the inventory
	auto room = [&](const ItemStack& slot) {
		int max = itemMaxStackSize(_gameData, slot);
		return !slot.isEmpty() && slot.sameItemSameComponents(stack) && max > 1 && slot.count < max;
	};
	if (room(_inventory.item(_player.getSelectedSlot())) || room(_inventory.item(40))) return true;
	int free = 0;
	for (int i = 0; i < 36; i++) {
		if (room(_inventory.item(i))) return true;
		if (_inventory.item(i).isEmpty()) free++;
	}
	return free > 0;
}

// moveItemToGrid: up to count of the item from the inventory into the slot; what is left, -1 if none is there
int AbstractFurnaceMenu::moveItemToGrid(Slot& slot, int item, int count) {
	ItemStack& current = slot.item();
	// Inventory.findSlotMatchingCraftingIngredient: the item, usable for crafting, the same as the slot's
	int found = -1;
	for (int i = 0; i < 36 && found < 0; i++) {
		const ItemStack& stack = _inventory.item(i);
		if (stack.isEmpty() || stack.item != item) continue;
		StackedItemContents usable;
		usable.accountSimpleStack(stack, _gameData);
		Ingredient only;
		only.items = {item};
		if (!usable.canCraft({only}) || (!current.isEmpty() && !current.sameItemSameComponents(stack))) continue;
		found = i;
	}
	if (found < 0) return -1;
	ItemStack taken = count < _inventory.item(found).count ? _inventory.removeItem(found, count) : _inventory.removeItemNoUpdate(found);
	int		  moved = taken.count;
	if (current.isEmpty()) {
		slot.set(std::move(taken));
	} else {
		current.grow(moved);
	}
	return count - moved;
}

// ===== BrewingStandMenu =====

BrewingStandMenu::BrewingStandMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId,
								   std::shared_ptr<BrewingStandBlockEntity> stand)
	: Menu(level, player, containerId, "minecraft:brewing_stand"), _inventory(inventory), _stand(std::move(stand)) {
	// PotionSlot: bottles, one per slot
	for (int i = 0; i < 3; i++) {
		Slot& potion	  = addSlot(*_stand, i);
		potion.placeRule  = [this](const ItemStack& stack) { return isPotionSlotItem(stack); };
		potion.stackLimit = [](const ItemStack&) { return 1; };
	}
	// IngredientsSlot and FuelSlot
	addSlot(*_stand, BrewingStandBlockEntity::INGREDIENT_SLOT).placeRule = [this](const ItemStack& stack) {
		return _level.potionBrewing().isIngredient(stack);
	};
	addSlot(*_stand, BrewingStandBlockEntity::FUEL_SLOT).placeRule = [this](const ItemStack& stack) { return isFuelSlotItem(stack); };
	addDataSlots(BrewingStandBlockEntity::NUM_DATA_VALUES);
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
}

bool BrewingStandMenu::isPotionSlotItem(const ItemStack& stack) const {
	return isItem(_gameData, stack, "minecraft:potion") || isItem(_gameData, stack, "minecraft:splash_potion") ||
		   isItem(_gameData, stack, "minecraft:lingering_potion") || isItem(_gameData, stack, "minecraft:glass_bottle");
}

bool BrewingStandMenu::isFuelSlotItem(const ItemStack& stack) const {
	return !stack.isEmpty() && _gameData.isInTag("minecraft:item", "minecraft:brewing_fuel", stack.item);
}

// Inventory: blaze powder into the fuel slot (else the ingredient slot), an ingredient into its slot, a bottle into
// the potion slots, else between the inventory and the hotbar. Stand slots: into the inventory, hotbar end first
ItemStack BrewingStandMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	Slot&	   ingredientSlot = _slots[BrewingStandBlockEntity::INGREDIENT_SLOT];
	if ((index < 0 || index > 2) && index != 3 && index != 4) {
		if (isFuelSlotItem(original)) {
			// Like vanilla: a fuel that moved into the fuel slot stops here
			if (moveItemStackTo(stack, 4, 5, false) || (ingredientSlot.mayPlace(stack, _gameData) && !moveItemStackTo(stack, 3, 4, false))) return {};
		} else if (ingredientSlot.mayPlace(stack, _gameData)) {
			if (!moveItemStackTo(stack, 3, 4, false)) return {};
		} else if (isPotionSlotItem(original)) {
			if (!moveItemStackTo(stack, 0, 3, false)) return {};
		} else if (index >= 5 && index < 32) {
			if (!moveItemStackTo(stack, 32, 41, false)) return {};
		} else if (index >= 32 && index < 41) {
			if (!moveItemStackTo(stack, 5, 32, false)) return {};
		} else if (!moveItemStackTo(stack, 5, 41, false)) {
			return {};
		}
	} else if (!moveItemStackTo(stack, 5, 41, true)) {
		return {};
	}
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	if (stack.count == original.count) return {};
	onTake(slot, original);
	return original;
}

// ===== Opening =====

namespace {
	std::vector<uint8_t> titleOf(const ContainerBlockEntity& entity) {
		if (!entity.customName.empty()) return entity.customName;
		Buffer name;
		TextComponent::writeTranslatable(name, entity.defaultName(), {});
		return name.getData();
	}
} // namespace

namespace Menus {
	void openFurnace(Player& player, Level& level, std::shared_ptr<AbstractFurnaceBlockEntity> furnace) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int					 id	   = player.nextContainerCounter();
		std::vector<uint8_t> title = titleOf(*furnace);
		open(player, level, std::make_unique<AbstractFurnaceMenu>(level, player, player.inventory(), id, std::move(furnace)), title);
	}

	void openBrewingStand(Player& player, Level& level, std::shared_ptr<BrewingStandBlockEntity> stand) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int					 id	   = player.nextContainerCounter();
		std::vector<uint8_t> title = titleOf(*stand);
		open(player, level, std::make_unique<BrewingStandMenu>(level, player, player.inventory(), id, std::move(stand)), title);
	}
} // namespace Menus
