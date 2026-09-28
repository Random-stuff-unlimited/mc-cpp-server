#ifndef PROCESSING_ENTITIES_HPP
#define PROCESSING_ENTITIES_HPP

#include "world/blockentity/ContainerEntities.hpp"
#include "world/item/Recipes.hpp"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

// The block entities that turn items into others: furnaces (furnace, blast furnace, smoker), brewing stands,
// campfires. Their blocks are in world/blocks/ProcessingBlocks.hpp, their menus in world/inventory/ProcessingMenus.hpp

// AbstractFurnaceBlockEntity: an input (0), a fuel (1) and a result (2) slot. While lit, the input cooks for the
// recipe's time; the fuel burns one item at a time, only when something can cook
class AbstractFurnaceBlockEntity : public ContainerBlockEntity {
  public:
	static constexpr int SLOT_INPUT			= 0;
	static constexpr int SLOT_FUEL			= 1;
	static constexpr int SLOT_RESULT		= 2;
	static constexpr int BURN_TIME_STANDARD = 200;
	static constexpr int BURN_COOL_SPEED	= 2;
	// ContainerData: lit time, lit duration, cooking progress, cooking total time
	static constexpr int NUM_DATA_VALUES = 4;

	// type: "minecraft:furnace", "minecraft:blast_furnace" or "minecraft:smoker"
	AbstractFurnaceBlockEntity(std::string type, const BlockPos& pos);

	std::string defaultName() const override;
	// The recipes it cooks: smelting, blasting or smoking
	RecipeType	recipeType() const { return _recipeType; }
	// The minecraft:menu entry of its screen
	std::string menuType() const { return type(); }

	int	 litTimeRemaining = 0; // lit_time_remaining
	int	 litTotalTime	  = 0; // lit_total_time
	int	 cookingTimer	  = 0; // cooking_time_spent
	int	 cookingTotalTime = 0; // cooking_total_time
	bool isLit() const { return litTimeRemaining > 0; }
	// dataAccess.get: the menu's data slots
	int	 data(int index) const;

	bool ticks() const override { return true; }
	void tick(Level& level) override; // serverTick

	// getBurnDuration: the fuel's burn time (halved in a blast furnace or smoker)
	int burnDuration(Level& level, const ItemStack& fuel) const;

	// Container / WorldlyContainer
	void			 setItem(int slot, ItemStack stack) override;
	bool			 canPlaceItem(int slot, const ItemStack& stack) const override;
	bool			 isWorldly() const override { return true; }
	std::vector<int> slotsForFace(Direction face) override;
	bool			 canPlaceItemThroughFace(int slot, const ItemStack& stack, const Direction*) override { return canPlaceItem(slot, stack); }
	bool			 canTakeItemThroughFace(int slot, const ItemStack& stack, Direction face) override;

	// RecipesUsed: how many times each recipe (by id) cooked since the experience was last given
	std::unordered_map<std::string, int> recipesUsed;
	void								 setRecipeUsed(const Recipe* recipe);
	// getRecipesToAwardAndPopExperience: the experience the recipes used are worth (ExperienceOrb.award). There are no
	// experience orbs yet: it is only counted, and recipesUsed is kept (saved) until orbs can take it
	int									 experienceToAward(Level& level) const;
	// awardUsedRecipesAndPopExperience: a player took the result (FurnaceResultSlot.checkTakeAchievements)
	void								 awardUsedRecipesAndPopExperience(Player& player);
	void								 preRemoveSideEffects(Level& level) override;

  protected:
	void saveExtra(BlockEntityWriter& out) const override;
	void loadExtra(BlockEntityReader& in) override;

  private:
	RecipeType			  _recipeType;
	mutable const Recipe* _lastRecipe = nullptr; // RecipeManager.CachedCheck (quickCheck)

	const Recipe* recipeFor(Level& level, const ItemStack& input) const;
	int			  totalCookTime(Level& level) const;
	bool		  canBurn(Level& level, const Recipe* recipe, const ItemStack& input) const;
	bool		  burn(Level& level, const Recipe* recipe, const ItemStack& input);
};

// BrewingStandBlockEntity: three bottles (0-2), an ingredient (3) and blaze powder (4). One blaze powder gives 20
// brews; a brew takes 400 ticks, and stops if the ingredient changes
class BrewingStandBlockEntity : public ContainerBlockEntity {
  public:
	static constexpr int INGREDIENT_SLOT = 3;
	static constexpr int FUEL_SLOT		 = 4;
	static constexpr int FUEL_USES		 = 20;
	static constexpr int BREW_TIME		 = 400;
	// ContainerData: brew time, fuel
	static constexpr int NUM_DATA_VALUES = 2;

	explicit BrewingStandBlockEntity(const BlockPos& pos) : ContainerBlockEntity("minecraft:brewing_stand", pos, 5) {}
	std::string defaultName() const override { return "container.brewing"; }

	int brewTime = 0; // BrewTime
	int fuel	 = 0; // Fuel
	int data(int index) const { return index == 0 ? brewTime : index == 1 ? fuel : 0; }

	bool ticks() const override { return true; }
	void tick(Level& level) override; // serverTick

	bool			 canPlaceItem(int slot, const ItemStack& stack) const override;
	bool			 isWorldly() const override { return true; }
	std::vector<int> slotsForFace(Direction face) override;
	bool			 canPlaceItemThroughFace(int slot, const ItemStack& stack, const Direction*) override { return canPlaceItem(slot, stack); }
	bool			 canTakeItemThroughFace(int slot, const ItemStack& stack, Direction face) override;

  protected:
	void saveExtra(BlockEntityWriter& out) const override;
	void loadExtra(BlockEntityReader& in) override;

  private:
	int								   _ingredient = 0; // The item being brewed with
	std::optional<std::array<bool, 3>> _lastPotionCount; // Which bottle slots the block shows (none before the first tick)

	bool isBrewable(Level& level) const;
	void doBrew(Level& level);
};

#endif
