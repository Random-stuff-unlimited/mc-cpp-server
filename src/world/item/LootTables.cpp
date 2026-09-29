#include "world/item/LootTables.hpp"

#include "data/GameData.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/blocks/Containers.hpp"
#include "world/blocks/StorageBlocks.hpp"
#include "world/item/Components.hpp"

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

void LootTables::loadEntities(const std::filesystem::path& file) {
	std::ifstream in(file);
	if (!in) throw std::runtime_error("cannot open " + file.string());
	json tables = json::parse(in);
	for (auto& [name, table] : tables.items()) _entityTables[name] = std::move(table);
}

std::vector<ItemStack> LootTables::blockDrops(Context& context) const { return this->drops(_byBlock[_gameData->getBlocks().blockOf(context.blockState)], context); }

std::vector<ItemStack> LootTables::entityDrops(const std::string& table, Context& context) const {
	auto it = _entityTables.find(table);
	return it == _entityTables.end() ? std::vector<ItemStack>() : this->drops(it->second, context);
}

std::vector<ItemStack> LootTables::drops(const json& table, Context& context) const {
	std::vector<ItemStack> drops;
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
	// ----- Container-like blocks: dynamic drops of the block entity (a cracked pot's sherds) -----
	if (entry.value("type", "") == "minecraft:dynamic") {
		if (context.blockEntity) StorageItems::dynamicDrops(*context.blockEntity, entry.value("name", ""), out, *_gameData);
		return;
	}
	// ----- End of container-like blocks -----
	if (entry.value("type", "") == "minecraft:loot_table") {
		// NestedLootTable: another table by name (or inline), its items getting this entry's functions and the pool's
		const json& value = entry["value"];
		const json* table = nullptr;
		if (value.is_string()) {
			auto it = _entityTables.find(value.get<std::string>());
			if (it != _entityTables.end()) table = &it->second;
		} else {
			table = &value;
		}
		if (!table) return;
		for (ItemStack stack : drops(*table, context)) {
			for (const json* functions : {&entry, &pool}) {
				if (!functions->contains("functions")) continue;
				for (const json& function : (*functions)["functions"]) {
					if (conditionsPass(function, context)) applyFunction(function, stack, context);
				}
			}
			out.push_back(std::move(stack));
		}
		return;
	}
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
	else if (name == "minecraft:copy_components" && context.blockEntity && function.value("source", "") == "block_entity") {
		// The block entity's components that are listed: a container's name and items
		ItemStack collected = stack.copyWithCount(stack.count);
		collected.components.clear();
		ContainerItems::collect(*context.blockEntity, collected, *_gameData);
		std::optional<ComponentPatch> from = ComponentPatch::parse(collected.components, *_gameData);
		if (!from) return;
		for (const json& include : function.value("include", json::array())) {
			int								   type	 = Components::typeId(*_gameData, include.get<std::string>());
			if (const std::vector<uint8_t>* value = from->get(type)) Components::set(stack, *_gameData, include.get<std::string>(), *value);
		}
	}
	else if (name == "minecraft:furnace_smelt" && !stack.isEmpty()) {
		// SmeltItemFunction: what smelting gives, as many as the stack
		CraftingInput input{1, 1, {stack}, 1};
		if (const Recipe* recipe = context.level.recipes().getRecipeFor(RecipeType::Smelting, input)) {
			ItemStack result = recipe->assemble(input);
			if (!result.isEmpty()) stack = result.copyWithCount(stack.count * result.count);
		}
	}
	// copy_state sets item components, not supported yet; enchanted_count_increase needs looting (0 for now)
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
	if (name == "minecraft:entity_properties") {
		if (!context.entity) return context.hasEntity; // Block tables: "this" is the player breaking it
		return entityMatches(c.value("predicate", json::object()), c.value("entity", "this"), context);
	}
	if (name == "minecraft:killed_by_player") return context.killedByPlayer;
	// Looting isn't known: its level is 0
	if (name == "minecraft:random_chance_with_enchanted_bonus") return random.nextFloat() < numberFloat(c["unenchanted_chance"], context);
	if (name == "minecraft:damage_source_properties") {
		if (!context.damage) return false;
		const json& predicate = c.value("predicate", json::object());
		for (const auto& [key, value] : predicate.items()) {
			if (key != "tags") return false; // Source entities (frogs, fireballs...) aren't ported: never them
			int type = _gameData->getSyncedId("minecraft:damage_type", context.damage->type);
			for (const json& tag : value) {
				if (_gameData->isInTag("minecraft:damage_type", tag.value("id", ""), type) != tag.value("expected", true)) return false;
			}
		}
		return true;
	}

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

bool LootTables::entityMatches(const json& predicate, const std::string& which, Context& context) const {
	// LootContext.EntityTarget: "this" (the entity that died), "attacker" (the one responsible), "direct_attacker"
	const Actor* actor = nullptr;
	if (which == "this") {
		actor = context.entity;
	} else if (context.damage) {
		actor = which == "attacker" ? context.damage->causing : context.damage->directEntity();
	}
	if (!actor) return false;
	const LivingEntity* living = actor->asLiving();
	for (const auto& [key, value] : predicate.items()) {
		if (key == "flags") {
			for (const auto& [flag, expected] : value.items()) {
				bool actual;
				if (flag == "is_on_fire") {
					if (!living) return false;
					actual = living->isOnFire();
				} else if (flag == "is_baby") {
					actual = living && living->isBaby();
				} else {
					return false;
				}
				if (actual != expected.get<bool>()) return false;
			}
		} else if (key == "type") {
			std::string type = value.get<std::string>();
			if (type.rfind('#', 0) == 0 ? !_gameData->isInTag("minecraft:entity_type", type.substr(1), actor->typeId())
										: _gameData->getStaticId("minecraft:entity_type", type) != actor->typeId()) {
				return false;
			}
		} else if (key == "components") {
			// The entity's components (a sheep's color, a chicken's variant): exactly these values
			if (!living) return false;
			for (const auto& [component, expected] : value.items()) {
				if (!expected.is_string() || living->lootComponent(component) != expected.get<std::string>()) return false;
			}
		} else if (key == "type_specific") {
			if (!living || !living->lootTypeSpecific(value)) return false;
		} else {
			return false; // Vehicles and the rest: not ported
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
