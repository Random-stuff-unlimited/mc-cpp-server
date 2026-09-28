#include "world/item/Recipes.hpp"

#include "data/GameData.hpp"
#include "lib/json.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "world/item/Components.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>
#include <unordered_map>

using json = nlohmann::json;

namespace {
	// StackedContents.canCraft for one craft: each stack goes to a different ingredient (a matching by augmenting
	// paths; the grid has at most 9 of each)
	bool canCraft(const std::vector<Ingredient>& ingredients, const std::vector<const ItemStack*>& stacks) {
		std::vector<int> owner(ingredients.size(), -1); // Stack given to each ingredient
		std::vector<bool> seen;
		auto			  assign = [&](auto& self, int stack) -> bool {
			 for (size_t i = 0; i < ingredients.size(); i++) {
				 if (seen[i] || !ingredients[i].test(*stacks[stack])) continue;
				 seen[i] = true;
				 if (owner[i] < 0 || self(self, owner[i])) {
					 owner[i] = stack;
					 return true;
				 }
			 }
			 return false;
		};
		for (int stack = 0; stack < static_cast<int>(stacks.size()); stack++) {
			seen.assign(ingredients.size(), false);
			if (!assign(assign, stack)) return false;
		}
		return true;
	}

	// TransmuteResult.apply: the input as the result item, its components kept, the result's own ones on top
	ItemStack transmute(const Recipe& recipe, const ItemStack& stack) {
		ItemStack result  = stack;
		result.item		  = recipe.result.item;
		result.count	  = recipe.result.count;
		if (!recipe.result.components.empty()) result.components = recipe.result.components; // None in vanilla
		return result;
	}
} // namespace


bool Ingredient::test(const ItemStack& stack) const { return !stack.isEmpty() && std::binary_search(items.begin(), items.end(), stack.item); }

CraftingInput::Positioned CraftingInput::ofPositioned(int width, int height, const std::vector<ItemStack>& grid) {
	int left = width - 1, right = 0, top = height - 1, bottom = 0;
	for (int y = 0; y < height; y++) {
		bool emptyRow = true;
		for (int x = 0; x < width; x++) {
			if (grid[x + y * width].isEmpty()) continue;
			left	 = std::min(left, x);
			right	 = std::max(right, x);
			emptyRow = false;
		}
		if (!emptyRow) {
			top	   = std::min(top, y);
			bottom = std::max(bottom, y);
		}
	}
	Positioned positioned;
	int		   cutWidth = right - left + 1, cutHeight = bottom - top + 1;
	if (width == 0 || height == 0 || cutWidth <= 0 || cutHeight <= 0) return positioned;
	positioned.left			= left;
	positioned.top			= top;
	CraftingInput& input	= positioned.input;
	input.width				= cutWidth;
	input.height			= cutHeight;
	for (int y = 0; y < cutHeight; y++) {
		for (int x = 0; x < cutWidth; x++) {
			const ItemStack& stack = grid[x + left + (y + top) * width];
			input.items.push_back(stack);
			if (!stack.isEmpty()) input.ingredientCount++;
		}
	}
	return positioned;
}

bool Recipe::matches(const CraftingInput& grid) const {
	switch (kind) {
	case Kind::Shaped: {
		// ShapedRecipePattern.matches: same size and count, as is or mirrored
		if (grid.ingredientCount != patternIngredients || grid.width != width || grid.height != height) return false;
		for (int mirrored = symmetrical ? 0 : 1; mirrored >= 0; mirrored--) {
			bool all = true;
			for (int y = 0; y < height && all; y++) {
				for (int x = 0; x < width && all; x++) {
					const std::optional<Ingredient>& cell  = pattern[(mirrored ? width - x - 1 : x) + y * width];
					const ItemStack&				 stack = grid.item(x, y);
					all								   = cell ? cell->test(stack) : stack.isEmpty();
				}
			}
			if (all) return true;
		}
		return false;
	}
	case Kind::Shapeless: {
		if (grid.ingredientCount != static_cast<int>(ingredients.size())) return false;
		if (grid.size() == 1 && ingredients.size() == 1) return ingredients[0].test(grid.items[0]);
		std::vector<const ItemStack*> stacks;
		for (const ItemStack& stack : grid.items) {
			if (!stack.isEmpty()) stacks.push_back(&stack);
		}
		return canCraft(ingredients, stacks);
	}
	case Kind::Transmute: {
		// The input once (not already the result) and the material once
		if (grid.ingredientCount != 2) return false;
		bool hasInput = false, hasMaterial = false;
		for (const ItemStack& stack : grid.items) {
			if (stack.isEmpty()) continue;
			if (!hasInput && input.test(stack)) {
				ItemStack result = transmute(*this, stack);
				if (result.count == 1 && result.sameItemSameComponents(stack)) return false; // isResultUnchanged
				hasInput = true;
			} else {
				if (hasMaterial || !material.test(stack)) return false;
				hasMaterial = true;
			}
		}
		return hasInput && hasMaterial;
	}
	case Kind::Cooking:
	case Kind::Stonecutting:
		// SingleItemRecipe: the one input item
		return grid.size() == 1 && input.test(grid.items[0]);
	case Kind::Other:
		return false;
	}
	return false;
}

ItemStack Recipe::assemble(const CraftingInput& grid) const {
	if (kind == Kind::Transmute) {
		for (const ItemStack& stack : grid.items) {
			if (input.test(stack)) return transmute(*this, stack);
		}
		return {};
	}
	return result;
}

std::vector<ItemStack> Recipe::remainingItems(const CraftingInput& grid, const GameData& gameData) const {
	std::vector<ItemStack> remaining(grid.items.size());
	for (size_t i = 0; i < grid.items.size(); i++) {
		if (grid.items[i].isEmpty()) continue;
		const GameData::ItemProperties* item = gameData.getItemProperties(grid.items[i].item);
		if (!item || item->craftingRemainder.empty()) continue;
		int remainder = gameData.getStaticId("minecraft:item", item->craftingRemainder);
		if (remainder > 0) remaining[i] = ItemStack(remainder, 1);
	}
	return remaining;
}
