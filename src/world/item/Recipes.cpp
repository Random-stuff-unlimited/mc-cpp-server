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
	json readJson(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file) throw std::runtime_error("can't open " + path.string());
		return json::parse(file);
	}

	std::string withNamespace(const std::string& id) { return id.find(':') == std::string::npos ? "minecraft:" + id : id; }

	int itemId(const GameData& gameData, const std::string& name) {
		int id = gameData.getStaticId("minecraft:item", withNamespace(name));
		if (id <= 0) throw std::runtime_error("unknown item " + name);
		return id;
	}

	// The item tags by name, while loading
	std::unordered_map<std::string, const std::vector<int>*> itemTags;

	// Ingredient.CODEC: "minecraft:stick", "#minecraft:planks" or a list of items
	Ingredient readIngredient(const json& value, const GameData& gameData) {
		Ingredient ingredient;
		if (value.is_array()) {
			for (const json& item : value) ingredient.items.push_back(itemId(gameData, item.get<std::string>()));
		} else {
			std::string name = value.get<std::string>();
			if (!name.empty() && name[0] == '#') {
				std::string tag	  = withNamespace(name.substr(1));
				auto		found = itemTags.find(tag);
				if (found == itemTags.end()) throw std::runtime_error("unknown item tag " + tag);
				ingredient.items = *found->second;
				ingredient.tag	 = tag;
			} else {
				ingredient.items.push_back(itemId(gameData, name));
			}
		}
		std::sort(ingredient.items.begin(), ingredient.items.end());
		ingredient.items.erase(std::unique(ingredient.items.begin(), ingredient.items.end()), ingredient.items.end());
		if (ingredient.items.empty()) throw std::runtime_error("empty ingredient");
		return ingredient;
	}

	// The components a recipe result may set, from JSON to their network format
	std::vector<uint8_t> encodeComponent(const std::string& type, const json& value, const GameData& gameData) {
		Buffer out;
		if (type == "minecraft:suspicious_stew_effects") {
			// SuspiciousStewEffects: the effects and their duration (160 ticks when not given)
			out.writeVarInt(static_cast<int32_t>(value.size()));
			for (const json& entry : value) {
				int effect = gameData.getStaticId("minecraft:mob_effect", withNamespace(entry.at("id").get<std::string>()));
				if (effect < 0) throw std::runtime_error("unknown effect " + entry.at("id").get<std::string>());
				out.writeVarInt(effect);
				out.writeVarInt(entry.value("duration", 160));
			}
		} else {
			throw std::runtime_error("result component " + type + " not supported");
		}
		return out.getData();
	}

	// ItemStack.STRICT_CODEC: {"id", "count" (1), "components"}
	ItemStack readResult(const json& value, const GameData& gameData) {
		if (value.is_string()) return ItemStack(itemId(gameData, value.get<std::string>()), 1);
		ItemStack result(itemId(gameData, value.at("id").get<std::string>()), value.value("count", 1));
		if (value.contains("components")) {
			for (const auto& [type, component] : value.at("components").items()) {
				int typeId = Components::typeId(gameData, withNamespace(type));
				if (typeId < 0) throw std::runtime_error("unknown component " + type);
				if (!Components::set(result, gameData, withNamespace(type), encodeComponent(withNamespace(type), component, gameData))) {
					throw std::runtime_error("can't set component " + type);
				}
			}
		}
		return result;
	}

	// ShapedRecipePattern.shrink: the pattern without its empty rows and columns around
	std::vector<std::string> shrink(const std::vector<std::string>& rows) {
		int first = INT32_MAX, last = 0, top = 0, emptyBottom = 0;
		for (int y = 0; y < static_cast<int>(rows.size()); y++) {
			const std::string& row		= rows[y];
			size_t			   nonSpace = row.find_first_not_of(' ');
			int				   start	= nonSpace == std::string::npos ? static_cast<int>(row.size()) : static_cast<int>(nonSpace);
			int				   end		= nonSpace == std::string::npos ? -1 : static_cast<int>(row.find_last_not_of(' '));
			first						= std::min(first, start);
			last						= std::max(last, end);
			if (end < 0) {
				if (top == y) top++;
				emptyBottom++;
			} else {
				emptyBottom = 0;
			}
		}
		if (static_cast<int>(rows.size()) == emptyBottom) return {};
		std::vector<std::string> shrunk;
		for (int y = top; y < static_cast<int>(rows.size()) - emptyBottom; y++) shrunk.push_back(rows[y].substr(first, last + 1 - first));
		return shrunk;
	}

	void readShaped(Recipe& recipe, const json& value, const GameData& gameData) {
		std::map<char, Ingredient> key;
		for (const auto& [symbol, ingredient] : value.at("key").items()) {
			if (symbol.size() != 1 || symbol == " ") throw std::runtime_error("invalid key symbol '" + symbol + "'");
			key[symbol[0]] = readIngredient(ingredient, gameData);
		}
		std::vector<std::string> rows = value.at("pattern").get<std::vector<std::string>>();
		if (rows.empty() || rows.size() > 3) throw std::runtime_error("pattern must have 1 to 3 rows");
		for (const std::string& row : rows) {
			if (row.size() > 3 || row.size() != rows[0].size()) throw std::runtime_error("pattern rows must be as wide, at most 3");
		}
		rows		  = shrink(rows);
		recipe.height = static_cast<int>(rows.size());
		recipe.width  = rows.empty() ? 0 : static_cast<int>(rows[0].size());
		for (const std::string& row : rows) {
			for (char symbol : row) {
				if (symbol == ' ') {
					recipe.pattern.emplace_back();
					continue;
				}
				auto found = key.find(symbol);
				if (found == key.end()) throw std::runtime_error(std::string("symbol '") + symbol + "' not in the key");
				recipe.pattern.emplace_back(found->second);
				recipe.patternIngredients++;
			}
		}
		// Util.isSymmetrical: each row reads the same both ways
		recipe.symmetrical = true;
		for (int y = 0; y < recipe.height && recipe.symmetrical; y++) {
			for (int x = 0; x < recipe.width / 2; x++) {
				if (recipe.pattern[x + y * recipe.width] != recipe.pattern[recipe.width - 1 - x + y * recipe.width]) recipe.symmetrical = false;
			}
		}
		recipe.result = readResult(value.at("result"), gameData);
	}

	std::unique_ptr<Recipe> readRecipe(const std::string& id, const json& value, const GameData& gameData) {
		auto recipe			   = std::make_unique<Recipe>();
		recipe->id			   = id;
		recipe->group		   = value.value("group", "");
		recipe->category	   = value.value("category", "misc");
		recipe->showNotification = value.value("show_notification", true);
		std::string	 type = withNamespace(value.at("type").get<std::string>());
		static const std::map<std::string, std::pair<RecipeType, int>> cooking = {
				{"minecraft:smelting", {RecipeType::Smelting, 200}},
				{"minecraft:blasting", {RecipeType::Blasting, 100}},
				{"minecraft:smoking", {RecipeType::Smoking, 100}},
				{"minecraft:campfire_cooking", {RecipeType::CampfireCooking, 100}},
		};
		if (type == "minecraft:crafting_shaped") {
			recipe->kind = Recipe::Kind::Shaped;
			readShaped(*recipe, value, gameData);
		} else if (type == "minecraft:crafting_shapeless") {
			recipe->kind = Recipe::Kind::Shapeless;
			for (const json& ingredient : value.at("ingredients")) recipe->ingredients.push_back(readIngredient(ingredient, gameData));
			if (recipe->ingredients.empty() || recipe->ingredients.size() > 9) throw std::runtime_error("1 to 9 ingredients");
			recipe->result = readResult(value.at("result"), gameData);
		} else if (type == "minecraft:crafting_transmute") {
			recipe->kind	 = Recipe::Kind::Transmute;
			recipe->input	 = readIngredient(value.at("input"), gameData);
			recipe->material = readIngredient(value.at("material"), gameData);
			recipe->result	 = readResult(value.at("result"), gameData);
		} else if (auto found = cooking.find(type); found != cooking.end()) {
			recipe->kind		= Recipe::Kind::Cooking;
			recipe->type		= found->second.first;
			recipe->input		= readIngredient(value.at("ingredient"), gameData);
			recipe->result		= readResult(value.at("result"), gameData);
			recipe->experience	= value.value("experience", 0.0f);
			recipe->cookingTime = value.value("cookingtime", found->second.second);
		} else if (type == "minecraft:stonecutting") {
			recipe->kind   = Recipe::Kind::Stonecutting;
			recipe->type   = RecipeType::Stonecutting;
			recipe->input  = readIngredient(value.at("ingredient"), gameData);
			recipe->result = readResult(value.at("result"), gameData);
		} else if (type.rfind("minecraft:smithing_", 0) == 0) {
			recipe->type = RecipeType::Smithing; // Not ported yet
		} else if (type.rfind("minecraft:crafting_", 0) != 0) {
			throw std::runtime_error("unknown recipe type " + type);
		} // The special crafting recipes (fireworks, map cloning...) aren't ported yet: they never match

		// PlacementInfo.create / createFromOptionals
		switch (recipe->kind) {
		case Recipe::Kind::Shaped:
			for (const std::optional<Ingredient>& cell : recipe->pattern) {
				recipe->slotsToIngredient.push_back(cell ? static_cast<int>(recipe->placement.size()) : -1);
				if (cell) recipe->placement.push_back(*cell);
			}
			break;
		case Recipe::Kind::Shapeless:
			recipe->placement = recipe->ingredients;
			break;
		case Recipe::Kind::Transmute:
			recipe->placement = {recipe->input, recipe->material};
			break;
		case Recipe::Kind::Cooking:
		case Recipe::Kind::Stonecutting:
			recipe->placement = {recipe->input};
			break;
		case Recipe::Kind::Other:
			break;
		}
		if (recipe->kind != Recipe::Kind::Shaped) {
			for (size_t i = 0; i < recipe->placement.size(); i++) recipe->slotsToIngredient.push_back(static_cast<int>(i));
		}
		return recipe;
	}

	void warn(const std::string& message) {
		if (g_logger) g_logger->logGameInfo(WARN, message, "Recipes");
	}

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

namespace {
	struct DisplayWriter {
		const GameData& gameData;
		int				slotType(const char* name) const { return gameData.getStaticId("minecraft:slot_display", name); }
		int				item(const char* name) const { return gameData.getStaticId("minecraft:item", name); }

		void itemSlot(Buffer& out, int item) const {
			out.writeVarInt(slotType("minecraft:item"));
			out.writeVarInt(item);
		}
		void stackSlot(Buffer& out, const ItemStack& stack) const {
			out.writeVarInt(slotType("minecraft:item_stack"));
			stack.write(out);
		}
		void emptySlot(Buffer& out) const { out.writeVarInt(slotType("minecraft:empty")); }
		// Ingredient.display: its tag, or each item (with what it leaves in the grid: displayForSingleItem)
		void ingredient(Buffer& out, const Ingredient& ingredient) const {
			if (!ingredient.tag.empty()) {
				out.writeVarInt(slotType("minecraft:tag"));
				out.writeString(ingredient.tag);
				return;
			}
			out.writeVarInt(slotType("minecraft:composite"));
			out.writeVarInt(static_cast<int32_t>(ingredient.items.size()));
			for (int id : ingredient.items) {
				const GameData::ItemProperties* properties = gameData.getItemProperties(id);
				int remainder = properties && !properties->craftingRemainder.empty() ? gameData.getStaticId("minecraft:item", properties->craftingRemainder) : -1;
				if (remainder > 0) {
					out.writeVarInt(slotType("minecraft:with_remainder"));
					itemSlot(out, id);
					stackSlot(out, ItemStack(remainder, 1));
				} else {
					itemSlot(out, id);
				}
			}
		}
		// Ingredient.CONTENTS_STREAM_CODEC (a holder set: 0 and the tag, or the count + 1 and the items)
		void contents(Buffer& out, const Ingredient& ingredient) const {
			if (!ingredient.tag.empty()) {
				out.writeVarInt(0);
				out.writeString(ingredient.tag);
				return;
			}
			out.writeVarInt(static_cast<int32_t>(ingredient.items.size()) + 1);
			for (int id : ingredient.items) out.writeVarInt(id);
		}
		// Recipe.display(): nullopt for the recipes the book doesn't show
		std::optional<std::vector<uint8_t>> display(const Recipe& recipe) const {
			Buffer out;
			auto   displayType = [&](const char* name) { out.writeVarInt(gameData.getStaticId("minecraft:recipe_display", name)); };
			switch (recipe.kind) {
			case Recipe::Kind::Shaped:
				displayType("minecraft:crafting_shaped");
				out.writeVarInt(recipe.width);
				out.writeVarInt(recipe.height);
				out.writeVarInt(static_cast<int32_t>(recipe.pattern.size()));
				for (const std::optional<Ingredient>& cell : recipe.pattern) cell ? ingredient(out, *cell) : emptySlot(out);
				stackSlot(out, recipe.result);
				itemSlot(out, item("minecraft:crafting_table"));
				break;
			case Recipe::Kind::Shapeless:
			case Recipe::Kind::Transmute:
				displayType("minecraft:crafting_shapeless");
				out.writeVarInt(static_cast<int32_t>(recipe.placement.size()));
				for (const Ingredient& each : recipe.placement) ingredient(out, each);
				stackSlot(out, recipe.result);
				itemSlot(out, item("minecraft:crafting_table"));
				break;
			case Recipe::Kind::Cooking: {
				static const char* stations[] = {nullptr, "minecraft:furnace", "minecraft:blast_furnace", "minecraft:smoker", "minecraft:campfire"};
				displayType("minecraft:furnace");
				ingredient(out, recipe.input);
				out.writeVarInt(slotType("minecraft:any_fuel"));
				stackSlot(out, recipe.result);
				itemSlot(out, item(stations[static_cast<int>(recipe.type)]));
				out.writeVarInt(recipe.cookingTime);
				out.writeFloat(recipe.experience);
				break;
			}
			case Recipe::Kind::Stonecutting:
				displayType("minecraft:stonecutter");
				ingredient(out, recipe.input);
				stackSlot(out, recipe.result);
				itemSlot(out, item("minecraft:stonecutter"));
				break;
			case Recipe::Kind::Other:
				return std::nullopt;
			}
			return out.getData();
		}
		// Recipe.recipeBookCategory
		int category(const Recipe& recipe) const {
			const std::string& c = recipe.category;
			const char*		   name;
			switch (recipe.type) {
			case RecipeType::Crafting:
				name = c == "building" ? "crafting_building_blocks" : c == "redstone" ? "crafting_redstone" : c == "equipment" ? "crafting_equipment" : "crafting_misc";
				break;
			case RecipeType::Smelting:
				name = c == "blocks" ? "furnace_blocks" : c == "food" ? "furnace_food" : "furnace_misc";
				break;
			case RecipeType::Blasting:
				name = c == "blocks" ? "blast_furnace_blocks" : "blast_furnace_misc";
				break;
			case RecipeType::Smoking:
				name = "smoker_food";
				break;
			case RecipeType::CampfireCooking:
				name = "campfire";
				break;
			case RecipeType::Stonecutting:
				name = "stonecutter";
				break;
			default:
				name = "smithing";
				break;
			}
			return gameData.getStaticId("minecraft:recipe_book_category", std::string("minecraft:") + name);
		}
	};
} // namespace

// RecipeManager.unpackRecipeInfo: each recipe's display, in id order; groups numbered as first seen
void RecipeManager::buildDisplays(const GameData& gameData) {
	DisplayWriter						 writer{gameData};
	std::unordered_map<std::string, int> groups;
	_displays.clear();
	for (const auto& recipe : _recipes) {
		std::optional<std::vector<uint8_t>> display = writer.display(*recipe);
		if (!display) continue;
		int group = -1;
		if (!recipe->group.empty()) group = groups.emplace(recipe->group, static_cast<int>(groups.size())).first->second;
		Buffer entry;
		entry.writeVarInt(static_cast<int32_t>(_displays.size()));
		entry.writeBytes(*display);
		entry.writeVarInt(group + 1); // OPTIONAL_VAR_INT
		entry.writeVarInt(writer.category(*recipe));
		entry.writeBool(true); // Crafting requirements (none only for the special recipes)
		entry.writeVarInt(static_cast<int32_t>(recipe->placement.size()));
		for (const Ingredient& ingredient : recipe->placement) writer.contents(entry, ingredient);
		_displays.push_back({recipe.get(), entry.getData(), std::move(*display)});
	}
}


void RecipeManager::load(const std::filesystem::path& extracted, const std::filesystem::path& overrides, const GameData& gameData) {
	// RecipeManager.prepare: a TreeMap by id, so each type lists its recipes in id order
	std::map<std::string, json> sources;
	json vanilla = readJson(extracted); // Kept: items() on a temporary would iterate over a destroyed object
	for (const auto& [id, recipe] : vanilla.items()) sources[id] = recipe;
	if (std::filesystem::is_directory(overrides)) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(overrides)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
			std::filesystem::path relative = std::filesystem::relative(entry.path(), overrides);
			relative.replace_extension();
			std::string path = relative.generic_string();
			size_t		slash = path.find('/');
			std::string id	  = slash == std::string::npos ? "minecraft:" + path : path.substr(0, slash) + ":" + path.substr(slash + 1);
			try {
				json recipe = readJson(entry.path());
				if (recipe.value("remove", false)) {
					sources.erase(id);
				} else {
					sources[id] = std::move(recipe);
				}
			} catch (const std::exception& e) {
				warn("Couldn't read recipe " + entry.path().string() + ": " + e.what());
			}
		}
	}

	itemTags.clear();
	for (const GameData::RegistryTags& registry : gameData.getTags()) {
		if (registry.registry != "minecraft:item") continue;
		for (const auto& [tagName, ids] : registry.tags) itemTags[tagName] = &ids;
	}
	_recipes.clear();
	_byId.clear();
	_byType.assign(static_cast<size_t>(RecipeType::Smithing) + 1, {});
	for (const auto& [id, value] : sources) {
		try {
			_recipes.push_back(readRecipe(id, value, gameData));
		} catch (const std::exception& e) {
			warn("Skipped recipe " + id + ": " + e.what());
			continue;
		}
		const Recipe* recipe = _recipes.back().get();
		_byId[id]			 = recipe;
		_byType[static_cast<size_t>(recipe->type)].push_back(recipe);
	}
	itemTags.clear();
	buildDisplays(gameData);
}

const Recipe* RecipeManager::byId(const std::string& id) const {
	auto found = _byId.find(id);
	return found == _byId.end() ? nullptr : found->second;
}

const Recipe* RecipeManager::getRecipeFor(RecipeType type, const CraftingInput& input, const Recipe* last) const {
	if (last && last->type == type && last->matches(input)) return last;
	if (input.isEmpty()) return nullptr; // RecipeMap.getRecipesFor
	for (const Recipe* recipe : _byType[static_cast<size_t>(type)]) {
		if (recipe->matches(input)) return recipe;
	}
	return nullptr;
}
