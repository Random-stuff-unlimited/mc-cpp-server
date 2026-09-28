#ifndef RECIPES_HPP
#define RECIPES_HPP

#include "world/item/ItemStack.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GameData;

// Vanilla's recipes (net.minecraft.world.item.crafting), read from gamedata/recipes.json (extracted from the game)
// and then from the recipes/ folder beside it: one datapack recipe per file, named after its id (acacia_boat.json is
// minecraft:acacia_boat, a namespace:path id is written namespace/path.json). Such a file replaces the vanilla recipe
// of that id, or adds one; {"remove": true} removes it

// Ingredient: the items that fit (an item, a list of items or an item tag); only the item counts, not its components
struct Ingredient {
	std::vector<int> items; // minecraft:item ids, sorted
	std::string		 tag;	// The item tag it was made from ("minecraft:planks"), empty for items

	bool test(const ItemStack& stack) const;
	bool operator==(const Ingredient& other) const { return items == other.items; }
};

// CraftingInput: the grid's items, cut to the smallest rectangle holding them all
struct CraftingInput {
	int					   width = 0, height = 0;
	std::vector<ItemStack> items;
	int					   ingredientCount = 0; // Non-empty stacks

	const ItemStack& item(int x, int y) const { return items[x + y * width]; }
	int				 size() const { return static_cast<int>(items.size()); }
	bool			 isEmpty() const { return ingredientCount == 0; }

	// CraftingInput.Positioned: the cut input and where it starts in the grid
	struct Positioned;
	// ofPositioned: from a width x height grid, row by row
	static Positioned ofPositioned(int width, int height, const std::vector<ItemStack>& grid);
};

struct CraftingInput::Positioned {
	CraftingInput input;
	int			  left = 0, top = 0;
};

// Recipe types (RecipeType), what each station looks for
enum class RecipeType { Crafting, Smelting, Blasting, Smoking, CampfireCooking, Stonecutting, Smithing };

struct Recipe {
	// The recipe serializers ported (the others are loaded but never match yet)
	enum class Kind { Shaped, Shapeless, Transmute, Cooking, Stonecutting, Other };

	std::string id; // "minecraft:acacia_boat"
	Kind		kind = Kind::Other;
	RecipeType	type = RecipeType::Crafting;
	std::string group;
	std::string category; // "building", "food"... (its recipe book tab)
	bool		showNotification = true;
	ItemStack	result;

	// Shaped: the pattern, row by row (nullopt = an empty cell)
	int								 width = 0, height = 0;
	std::vector<std::optional<Ingredient>> pattern;
	int								 patternIngredients = 0;
	bool							 symmetrical		= false; // Same mirrored: tested once
	// Shapeless: the ingredients, in any cell
	std::vector<Ingredient> ingredients;
	// Transmute (input + material: the input becomes the result, keeping its components), cooking and stonecutting
	// (input)
	Ingredient input, material;
	// Cooking
	float experience  = 0;
	int	  cookingTime = 0;

	// PlacementInfo: the ingredients the recipe book looks for (the pattern's cells that aren't empty), and which one
	// each cell of the pattern takes (-1 for none). No ingredients: it can't be placed
	std::vector<Ingredient> placement;
	std::vector<int>		slotsToIngredient;

	bool	  matches(const CraftingInput& input) const;
	ItemStack assemble(const CraftingInput& input) const;
	// getRemainingItems: what stays in each cell (buckets, bottles), CraftingRecipe.defaultCraftingReminder
	std::vector<ItemStack> remainingItems(const CraftingInput& input, const GameData& gameData) const;
};

// StackedItemContents: the items at hand (the inventory and the grid) counted by item, to find which of them can
// make a recipe and how many times (StackedContents.RecipePicker: assigns items to ingredients by augmenting paths)
class StackedItemContents {
  public:
	// accountSimpleStack: not damaged, enchanted or renamed (Inventory.isUsableForCrafting)
	void accountSimpleStack(const ItemStack& stack, const GameData& gameData);
	void accountStack(const ItemStack& stack, int max);
	// canCraft: `times` crafts at once; `picked` gets the item used for each ingredient
	bool canCraft(const std::vector<Ingredient>& ingredients, int times = 1, std::vector<int>* picked = nullptr);
	// getBiggestCraftableStack: how many crafts at once, at most `max`
	int	 biggestCraftableStack(const std::vector<Ingredient>& ingredients, int max = INT32_MAX, std::vector<int>* picked = nullptr);

  private:
	std::vector<std::pair<int, int>> _amounts; // Item, count; in the order first seen

	int	 amount(int item) const;
	void add(int item, int count);
	bool tryPick(const std::vector<Ingredient>& ingredients, const std::vector<int>& items, int times, std::vector<int>* picked);
};

// RecipeManager: every recipe by type, in id order (the first matching one wins)
class RecipeManager {
  public:
	// RecipeManager.ServerDisplayInfo: how the recipe book shows a recipe, its index being its RecipeDisplayId
	struct DisplayInfo {
		const Recipe*		 recipe;
		std::vector<uint8_t> entry;	  // RecipeDisplayEntry as sent (id, display, group, category, requirements)
		std::vector<uint8_t> display; // Its RecipeDisplay alone (the ghost recipe)
	};

	void load(const std::filesystem::path& extracted, const std::filesystem::path& overrides, const GameData& gameData);

	const Recipe* byId(const std::string& id) const;
	// getRecipeFor: the last recipe used again if it still matches, else the first matching one; nullptr if none
	const Recipe* getRecipeFor(RecipeType type, const CraftingInput& input, const Recipe* last = nullptr) const;
	size_t		  size() const { return _recipes.size(); }
	const std::vector<DisplayInfo>& displays() const { return _displays; }
	const DisplayInfo*				display(int id) const { return id >= 0 && id < static_cast<int>(_displays.size()) ? &_displays[id] : nullptr; }

  private:
	std::vector<std::unique_ptr<Recipe>>		  _recipes; // By id
	std::unordered_map<std::string, const Recipe*> _byId;
	std::vector<std::vector<const Recipe*>>		  _byType;
	std::vector<DisplayInfo>					  _displays;

	void buildDisplays(const GameData& gameData);
};

#endif
