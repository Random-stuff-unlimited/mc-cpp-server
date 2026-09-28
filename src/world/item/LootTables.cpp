#include "world/item/LootTables.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

using json = nlohmann::json;

void LootTables::load(const std::filesystem::path& file, const GameData& gameData) {
	_gameData = &gameData;
	std::ifstream in(file);
	if (!in) throw std::runtime_error("cannot open " + file.string());
	json tables = json::parse(in);
	_byBlock.assign(gameData.getBlockCount(), json());
	for (auto& [name, table] : tables.items()) {
		int block = gameData.getStaticId("minecraft:block", name);
		if (block >= 0) _byBlock[block] = std::move(table);
	}
}

std::vector<ItemStack> LootTables::blockDrops(Context& context) const {
	std::vector<ItemStack> drops;
	const json&			   table = _byBlock[_gameData->getBlocks().blockOf(context.blockState)];
	if (table.is_null() || !table.contains("pools")) return drops;

	for (const json& pool : table["pools"]) {
		if (!conditionsPass(pool, context)) continue;
		int rolls = numberInt(pool.value("rolls", json(1)), context); // Luck is 0: bonus_rolls never add any
		for (int roll = 0; roll < rolls; roll++) {
			// LootPool.addRandomItem: the entries that apply, then one of them by weight
			std::vector<const json*> entries;
			for (const json& entry : pool["entries"]) expand(entry, context, entries);
			int total = 0;
			for (const json* entry : entries) total += std::max(entry->value("weight", 1), 0);
			if (total == 0 || entries.empty()) continue;
			if (entries.size() == 1) {
				createItems(*entries[0], pool, context, drops);
				continue;
			}
			int pick = context.level.random().nextInt(total);
			for (const json* entry : entries) {
				pick -= std::max(entry->value("weight", 1), 0);
				if (pick < 0) {
					createItems(*entry, pool, context, drops);
					break;
				}
			}
		}
	}

	// LootTable.createStackSplitter: stacks bigger than the item's maximum are split
	std::vector<ItemStack> split;
	for (ItemStack& stack : drops) {
		if (stack.isEmpty()) continue;
		const GameData::ItemProperties* item = _gameData->getItemProperties(stack.item);
		int								max	 = item ? item->maxStackSize : 64;
		while (stack.count > max) {
			split.push_back(stack.copyWithCount(max));
			stack.count -= max;
		}
		split.push_back(stack);
	}
	return split;
}

bool LootTables::expand(const json& entry, Context& context, std::vector<const json*>& out) const {
	if (!conditionsPass(entry, context)) return false;
	std::string type = entry.value("type", "");
	if (type == "minecraft:alternatives") {
		for (const json& child : entry["children"]) {
			if (expand(child, context, out)) return true;
		}
		return false;
	}
	if (type == "minecraft:sequence") {
		for (const json& child : entry["children"]) {
			if (!expand(child, context, out)) return false;
		}
		return true;
	}
	if (type == "minecraft:group") {
		for (const json& child : entry["children"]) expand(child, context, out);
		return true;
	}
	// Single entries: item, empty, and the ones not supported (dynamic contents) that give nothing
	out.push_back(&entry);
	return true;
}

void LootTables::createItems(const json& entry, const json& pool, Context& context, std::vector<ItemStack>& out) const {
	if (entry.value("type", "") != "minecraft:item") return;
	int item = _gameData->getStaticId("minecraft:item", entry.value("name", ""));
	if (item <= 0) return;
	ItemStack stack(item, 1);
	// The entry's functions, then the pool's
	for (const json* functions : {&entry, &pool}) {
		if (!functions->contains("functions")) continue;
		for (const json& function : (*functions)["functions"]) {
			if (conditionsPass(function, context)) applyFunction(function, stack, context);
		}
	}
	out.push_back(std::move(stack));
}

void LootTables::applyFunction(const json& function, ItemStack& stack, Context& context) const {
	std::string name   = function.value("function", "");
	JavaRandom& random = context.level.random();
	if (name == "minecraft:set_count") {
		int count	= numberInt(function["count"], context);
		stack.count = function.value("add", false) ? stack.count + count : count;
	} else if (name == "minecraft:explosion_decay") {
		if (context.explosionRadius > 0.0f) {
			float chance = 1.0f / context.explosionRadius;
			int	  kept	 = 0;
			for (int i = 0; i < stack.count; i++) {
				if (random.nextFloat() <= chance) kept++;
			}
			stack.count = kept;
		}
	} else if (name == "minecraft:apply_bonus") {
		// Enchantment level 0 (fortune isn't known yet)
		std::string formula	   = function.value("formula", "");
		const json& parameters = function.value("parameters", json::object());
		if (formula == "minecraft:uniform_bonus_count") {
			stack.count += random.nextInt(parameters.value("bonusMultiplier", 1) * 0 + 1);
		} else if (formula == "minecraft:binomial_with_bonus_count") {
			int	  extra		  = parameters.value("extra", 0);
			float probability = parameters.value("probability", 0.0f);
			for (int i = 0; i < extra; i++) {
				if (random.nextFloat() < probability) stack.count++;
			}
		}
		// minecraft:ore_drops only changes anything with fortune
	} else if (name == "minecraft:limit_count") {
		const json& limit = function["limit"];
		if (limit.contains("min")) stack.count = std::max(stack.count, numberInt(limit["min"], context));
		if (limit.contains("max")) stack.count = std::min(stack.count, numberInt(limit["max"], context));
	}
	// copy_components and copy_state set item components, not supported yet
}

bool LootTables::conditionsPass(const json& holder, Context& context) const {
	if (!holder.contains("conditions")) return true;
	for (const json& condition : holder["conditions"]) {
		if (!this->condition(condition, context)) return false;
	}
	return true;
}

bool LootTables::condition(const json& c, Context& context) const {
	std::string name   = c.value("condition", "");
	JavaRandom& random = context.level.random();
	if (name == "minecraft:survives_explosion") return context.explosionRadius <= 0.0f || random.nextFloat() <= 1.0f / context.explosionRadius;
	if (name == "minecraft:inverted") return !condition(c["term"], context);
	if (name == "minecraft:any_of") {
		for (const json& term : c["terms"]) {
			if (condition(term, context)) return true;
		}
		return false;
	}
	if (name == "minecraft:all_of") {
		for (const json& term : c["terms"]) {
			if (!condition(term, context)) return false;
		}
		return true;
	}
	if (name == "minecraft:random_chance") return random.nextFloat() < numberFloat(c["chance"], context);
	if (name == "minecraft:table_bonus") return random.nextFloat() < c["chances"].at(0).get<float>(); // Enchantment level 0
	if (name == "minecraft:match_tool") return context.tool && !context.tool->isEmpty() && toolMatches(c["predicate"], context);
	if (name == "minecraft:entity_properties") return context.hasEntity;

	// Block and state checks: the table's block itself, or the block at an offset (location_check)
	int state = context.blockState;
	const json* predicate = &c;
	if (name == "minecraft:location_check") {
		BlockPos pos{context.origin.x + c.value("offsetX", 0), context.origin.y + c.value("offsetY", 0), context.origin.z + c.value("offsetZ", 0)};
		state	  = context.level.getBlockState(pos);
		predicate = &c["predicate"]["block"];
		std::string blocks = (*predicate).value("blocks", "");
		if (!blocks.empty() && _gameData->getStaticId("minecraft:block", blocks) != _gameData->getBlocks().blockOf(state)) return false;
	} else if (name == "minecraft:block_state_property") {
		if (_gameData->getStaticId("minecraft:block", c.value("block", "")) != _gameData->getBlocks().blockOf(state)) return false;
	} else {
		return false; // Unknown condition: nothing drops through it
	}
	const json& properties = predicate->contains("properties") ? (*predicate)["properties"] : predicate->value("state", json::object());
	const BlockRegistry& blocks = _gameData->getBlocks();
	for (const auto& [property, expected] : properties.items()) {
		int id = blocks.property(property);
		if (id < 0 || blocks.get(state, id) < 0) return false;
		const std::string& value = blocks.valueName(blocks.get(state, id));
		if (expected.is_string()) {
			if (value != expected.get<std::string>()) return false;
		} else {
			int	 number = blocks.valueAsInt(blocks.get(state, id));
			auto bound	= [](const json& v) { return v.is_string() ? std::stoi(v.get<std::string>()) : v.get<int>(); };
			if (expected.contains("min") && number < bound(expected["min"])) return false;
			if (expected.contains("max") && number > bound(expected["max"])) return false;
		}
	}
	return true;
}

// ItemPredicate: the items, and nothing about enchantments (not known yet: such predicates fail)
bool LootTables::toolMatches(const json& predicate, Context& context) const {
	if (predicate.contains("predicates") || predicate.contains("components")) return false;
	if (!predicate.contains("items")) return true;
	std::vector<std::string> items;
	if (predicate["items"].is_array()) {
		items = predicate["items"].get<std::vector<std::string>>();
	} else {
		items.push_back(predicate["items"].get<std::string>());
	}
	for (const std::string& item : items) {
		if (item.rfind('#', 0) == 0 ? _gameData->isInTag("minecraft:item", item.substr(1), context.tool->item)
									: _gameData->getStaticId("minecraft:item", item) == context.tool->item) {
			return true;
		}
	}
	return false;
}

// Number providers: a number, or constant, uniform and binomial
float LootTables::numberFloat(const json& number, Context& context) const {
	if (number.is_number()) return number.get<float>();
	std::string type = number.value("type", "minecraft:constant");
	if (type == "minecraft:uniform") {
		float min = numberFloat(number["min"], context), max = numberFloat(number["max"], context);
		return context.level.random().nextFloat() * (max - min) + min;
	}
	if (type == "minecraft:binomial") return static_cast<float>(numberInt(number, context));
	return number.value("value", 0.0f);
}

int LootTables::numberInt(const json& number, Context& context) const {
	if (number.is_number()) return static_cast<int>(std::lround(number.get<float>()));
	std::string type = number.value("type", "minecraft:constant");
	if (type == "minecraft:uniform") {
		int min = numberInt(number["min"], context), max = numberInt(number["max"], context);
		return min >= max ? min : context.level.random().nextInt(max - min + 1) + min;
	}
	if (type == "minecraft:binomial") {
		int	  n		  = numberInt(number["n"], context);
		float p		  = numberFloat(number["p"], context);
		int	  success = 0;
		for (int i = 0; i < n; i++) {
			if (context.level.random().nextFloat() < p) success++;
		}
		return success;
	}
	return static_cast<int>(std::lround(number.value("value", 0.0f)));
}
