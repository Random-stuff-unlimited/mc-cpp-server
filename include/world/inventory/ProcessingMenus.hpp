#ifndef PROCESSING_MENUS_HPP
#define PROCESSING_MENUS_HPP

#include "world/blockentity/ProcessingEntities.hpp"
#include "world/inventory/Menu.hpp"

#include <memory>

// The menus of the block entities that process items (world/blockentity/ProcessingEntities.hpp)

// AbstractFurnaceMenu (FurnaceMenu, BlastFurnaceMenu, SmokerMenu): input 0, fuel 1 (FurnaceFuelSlot), result 2
// (FurnaceResultSlot), then the player's inventory; 4 data slots (lit time, lit duration, cooking progress, cooking
// total time). A recipe book menu: the book places a recipe's ingredient in the input slot
class AbstractFurnaceMenu : public Menu {
  public:
	AbstractFurnaceMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, std::shared_ptr<AbstractFurnaceBlockEntity> furnace);
	ItemStack  quickMoveStack(int slot) override;
	bool	   stillValid() override { return _furnace->stillValid(_player); }
	RecipeType recipeType() const { return _furnace->recipeType(); }
	// RecipeBookType: 1 furnace, 2 blast furnace, 3 smoker (crafting is 0)
	int		   recipeBookType() const;
	// handlePlacement (ServerPlaceRecipe.placeRecipe with a 1x1 grid): true when the book should show a ghost recipe
	bool	   handlePlacement(const Recipe& recipe, bool useMaxItems, bool creative);

  protected:
	int	 dataSlot(int index) const override { return _furnace->data(index); }
	void onTake(Slot& slot, const ItemStack& stack) override;

  private:
	PlayerContainer								_inventory;
	std::shared_ptr<AbstractFurnaceBlockEntity> _furnace;

	// canSmelt (the recipe type's accepted inputs) and isFuel
	bool canSmelt(const ItemStack& stack) const;
	bool isFuel(const ItemStack& stack) const;
	// ServerPlaceRecipe helpers over the input slot
	bool testClearGrid();
	void clearGrid();
	int	 moveItemToGrid(Slot& slot, int item, int count);
};

// BrewingStandMenu: three potion slots (PotionSlot: bottles, one each), the ingredient (IngredientsSlot) and the fuel
// (FuelSlot), then the player's inventory; 2 data slots (brew time, fuel)
class BrewingStandMenu : public Menu {
  public:
	BrewingStandMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, std::shared_ptr<BrewingStandBlockEntity> stand);
	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override { return _stand->stillValid(_player); }

  protected:
	int dataSlot(int index) const override { return _stand->data(index); }

  private:
	PlayerContainer							 _inventory;
	std::shared_ptr<BrewingStandBlockEntity> _stand;

	bool isPotionSlotItem(const ItemStack& stack) const; // PotionSlot.mayPlaceItem
	bool isFuelSlotItem(const ItemStack& stack) const;	 // FuelSlot.mayPlaceItem
};

namespace Menus {
	// Opens a furnace's or brewing stand's menu, titled with its custom name or its default one
	void openFurnace(Player& player, Level& level, std::shared_ptr<AbstractFurnaceBlockEntity> furnace);
	void openBrewingStand(Player& player, Level& level, std::shared_ptr<BrewingStandBlockEntity> stand);
} // namespace Menus

#endif
