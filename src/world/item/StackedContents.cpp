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

// ----- StackedItemContents -----

void StackedItemContents::accountSimpleStack(const ItemStack& stack, const GameData& gameData) {
	if (stack.isEmpty()) return;
	if (!stack.components.empty()) {
		// Inventory.isUsableForCrafting: not damaged, not enchanted, not renamed (a patch that can't be read: left out)
		std::optional<ComponentPatch> patch = ComponentPatch::parse(stack.components, gameData);
		if (!patch) return;
		const std::vector<uint8_t>* damage		 = patch->get(Components::typeId(gameData, "minecraft:damage"));
		const std::vector<uint8_t>* enchantments = patch->get(Components::typeId(gameData, "minecraft:enchantments"));
		if (damage && !damage->empty() && (*damage)[0] != 0) return;
		if (enchantments && !enchantments->empty() && (*enchantments)[0] != 0) return;
		if (patch->get(Components::typeId(gameData, "minecraft:custom_name"))) return;
	}
	const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
	accountStack(stack, item ? item->maxStackSize : 64);
}

void StackedItemContents::accountStack(const ItemStack& stack, int max) {
	if (!stack.isEmpty()) add(stack.item, std::min(max, stack.count));
}

int StackedItemContents::amount(int item) const {
	for (const auto& [id, count] : _amounts) {
		if (id == item) return count;
	}
	return 0;
}

void StackedItemContents::add(int item, int count) {
	for (auto& [id, amount] : _amounts) {
		if (id == item) {
			amount += count;
			return;
		}
	}
	_amounts.emplace_back(item, count);
}

// RecipePicker.tryPick: assigns one item kind to each ingredient, `times` of it each, through augmenting paths
// (items and ingredients alternate on a path; an item's count is taken when it is assigned)
bool StackedItemContents::tryPick(const std::vector<Ingredient>& ingredients, const std::vector<int>& items, int times, std::vector<int>* picked) {
	if (times <= 0) return true;
	int				   ingredientCount = static_cast<int>(ingredients.size());
	int				   itemCount	   = static_cast<int>(items.size());
	std::vector<bool>  connection(static_cast<size_t>(itemCount * ingredientCount)), assigned(connection.size());
	std::vector<bool>  satisfied(ingredientCount), visitedIngredient(ingredientCount), visitedItem(itemCount);
	for (int ingredient = 0; ingredient < ingredientCount; ingredient++) {
		for (int item = 0; item < itemCount; item++) {
			if (std::binary_search(ingredients[ingredient].items.begin(), ingredients[ingredient].items.end(), items[item])) {
				connection[item * ingredientCount + ingredient] = true;
			}
		}
	}
	auto takeAmount = [&](int item, int count) {
		for (auto& [id, amount] : _amounts) {
			if (id == item) amount -= count;
		}
	};
	std::vector<int> path;
	auto findPath = [&](int start) -> bool {
		path.clear();
		visitedItem[start] = true;
		path.push_back(start);
		while (!path.empty()) {
			size_t size = path.size();
			if (((size - 1) & 1) == 0) { // An item: to an ingredient it can go to
				int item = path.back();
				for (int ingredient = 0; ingredient < ingredientCount; ingredient++) {
					if (!visitedIngredient[ingredient] && connection[item * ingredientCount + ingredient] && !assigned[item * ingredientCount + ingredient]) {
						visitedIngredient[ingredient] = true;
						path.push_back(ingredient);
						break;
					}
				}
			} else { // An ingredient: done if free, else on to the item it has
				int ingredient = path.back();
				if (!satisfied[ingredient]) return true;
				for (int item = 0; item < itemCount; item++) {
					if (!visitedItem[item] && assigned[item * ingredientCount + ingredient]) {
						visitedItem[item] = true;
						path.push_back(item);
						break;
					}
				}
			}
			if (path.size() == size) path.pop_back();
		}
		return false;
	};
	int count = 0;
	while (true) {
		// tryAssigningNewItem
		std::fill(visitedIngredient.begin(), visitedIngredient.end(), false);
		std::fill(visitedItem.begin(), visitedItem.end(), false);
		bool found = false;
		for (int item = 0; item < itemCount && !found; item++) {
			if (amount(items[item]) >= times) found = findPath(item);
		}
		if (!found) break;
		takeAmount(items[path[0]], times);
		satisfied[path.back()] = true;
		count++;
		for (size_t i = 0; i + 1 < path.size(); i++) {
			if ((i & 1) == 0) {
				assigned[path[i] * ingredientCount + path[i + 1]] = true;
			} else {
				assigned[path[i + 1] * ingredientCount + path[i]] = false;
			}
		}
	}
	bool all = count == ingredientCount;
	if (picked && all) picked->clear();
	// Puts everything back, telling the item picked for each ingredient
	for (int ingredient = 0; ingredient < ingredientCount; ingredient++) {
		for (int item = 0; item < itemCount; item++) {
			if (!assigned[item * ingredientCount + ingredient]) continue;
			takeAmount(items[item], -times);
			if (picked && all) picked->push_back(items[item]);
			break;
		}
	}
	return all;
}

bool StackedItemContents::canCraft(const std::vector<Ingredient>& ingredients, int times, std::vector<int>* picked) {
	if (ingredients.empty()) return false; // isImpossibleToPlace
	// getUniqueAvailableIngredientItems
	std::vector<int> items;
	for (const auto& [item, count] : _amounts) {
		if (count <= 0) continue;
		for (const Ingredient& ingredient : ingredients) {
			if (std::binary_search(ingredient.items.begin(), ingredient.items.end(), item)) {
				items.push_back(item);
				break;
			}
		}
	}
	return tryPick(ingredients, items, times, picked);
}

int StackedItemContents::biggestCraftableStack(const std::vector<Ingredient>& ingredients, int max, std::vector<int>* picked) {
	std::vector<int> items;
	for (const auto& [item, count] : _amounts) {
		if (count <= 0) continue;
		for (const Ingredient& ingredient : ingredients) {
			if (std::binary_search(ingredient.items.begin(), ingredient.items.end(), item)) {
				items.push_back(item);
				break;
			}
		}
	}
	// getResultUpperBound: the fewest of the most available item of each ingredient
	int upper = INT32_MAX;
	for (const Ingredient& ingredient : ingredients) {
		int most = 0;
		for (const auto& [item, count] : _amounts) {
			if (count > most && std::binary_search(ingredient.items.begin(), ingredient.items.end(), item)) most = count;
		}
		upper = std::min(upper, most);
		if (most == 0) break;
	}
	// tryPickAll: a binary search on the count
	int low = 0, high = static_cast<int>(std::min<int64_t>(max, upper) + 1);
	while (true) {
		int middle = (low + high) / 2;
		if (tryPick(ingredients, items, middle, nullptr)) {
			if (high - low <= 1) {
				if (middle > 0) tryPick(ingredients, items, middle, picked);
				return middle;
			}
			low = middle;
		} else {
			high = middle;
		}
	}
}

