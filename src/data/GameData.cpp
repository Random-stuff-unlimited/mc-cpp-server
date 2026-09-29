#include "data/GameData.hpp"

#include "lib/json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

// ordered_json keeps the file order, which is the order registries are sent in
using json = nlohmann::ordered_json;

namespace {
	// JSON name -> field: the same names are used by blocks.json, block_states.json and overrides.json
	const std::map<std::string, float GameData::BlockProperties::*> BLOCK_PROPERTIES = {
			{"destroy_time", &GameData::BlockProperties::destroyTime},
			{"explosion_resistance", &GameData::BlockProperties::explosionResistance},
			{"friction", &GameData::BlockProperties::friction},
			{"speed_factor", &GameData::BlockProperties::speedFactor},
			{"jump_factor", &GameData::BlockProperties::jumpFactor},
	};

	void setStateProperty(GameData::StateProperties& state, const std::string& property, double value) {
		if (property == "light_emission") {
			if (value < 0 || value > 15) throw std::runtime_error("light_emission must be between 0 and 15");
			state.lightEmission = static_cast<uint8_t>(value);
		} else if (property == "requires_correct_tool") {
			state.requiresCorrectTool = value != 0;
		} else if (property == "occludes") {
			state.occludes = value != 0;
		} else if (property == "light_block") {
			if (value < 0 || value > 15) throw std::runtime_error("light_block must be between 0 and 15");
			state.lightBlock = static_cast<uint8_t>(value);
		} else if (property == "propagates_skylight_down") {
			state.propagatesSkylightDown = value != 0;
		} else if (property == "blocks_motion") {
			state.blocksMotion = value != 0;
		} else if (property == "solid") {
			state.solid = value != 0;
		} else if (property == "replaceable") {
			state.replaceable = value != 0;
		} else if (property == "random_ticking") {
			state.randomTicking = value != 0;
		} else if (property == "liquid") {
			state.liquid = value != 0;
		} else if (property == "solid_render") {
			state.solidRender = value != 0;
		} else if (property == "use_shape_for_light_occlusion") {
			state.useShapeForLightOcclusion = value != 0;
		} else if (property == "ignited_by_lava") {
			state.ignitedByLava = value != 0;
		} else if (property == "analog_output") {
			state.analogOutput = value != 0;
		} else if (property == "push_reaction") {
			state.pushReaction = static_cast<uint8_t>(value);
		} else if (property == "pathfindable") {
			state.pathfindable = static_cast<uint8_t>(value);
		} else if (property == "suffocating") {
			state.suffocating = value != 0;
		} else if (property == "valid_spawn") {
			state.validSpawn = static_cast<uint8_t>(value);
		} else if (property == "occlusion_shape") {
			state.occlusionShape = static_cast<uint16_t>(value);
		} else if (property == "redstone_conductor") {
			state.redstoneConductor = value != 0;
		} else if (property == "face_sturdy") {
			state.faceSturdy = static_cast<uint32_t>(value);
		} else if (property == "collision_shape") {
			state.collisionShape = static_cast<uint16_t>(value);
		} else if (property == "fluid") {
			state.fluid = static_cast<uint8_t>(value);
		} else if (property == "fluid_amount") {
			state.fluidAmount = static_cast<uint8_t>(value);
		} else if (property == "fluid_falling") {
			state.fluidFalling = value != 0;
		} else {
			throw std::runtime_error("unknown block property \"" + property + "\"");
		}
	}

	// Numbers, or true/false for yes/no properties
	double toNumber(const json& value) {
		if (value.is_boolean()) return value.get<bool>() ? 1 : 0;
		if (!value.is_number()) throw std::runtime_error("expected a number or true/false, got " + value.dump());
		return value.get<double>();
	}

	json readJson(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file.is_open()) throw std::runtime_error("Cannot open " + path.string() + " (run tools/update_gamedata.py)");
		return json::parse(file);
	}

	GameData::Registry makeRegistry(const std::string& name, const json& entries) {
		GameData::Registry registry;
		registry.name	 = name;
		registry.entries = entries.get<std::vector<std::string>>();
		for (size_t i = 0; i < registry.entries.size(); i++) {
			registry.ids[registry.entries[i]] = static_cast<int>(i);
		}
		return registry;
	}
} // namespace

void GameData::load(const std::filesystem::path& directory) {
	_directory = directory;
	json version	 = readJson(directory / "version.json");
	_versionName	 = version.at("name").get<std::string>();
	_protocolVersion = version.at("protocol").get<int>();
	_dataVersion	 = version.at("data_version").get<int>();

	// Each file is kept in a variable: items() on a temporary would iterate over a destroyed object
	json staticRegistries = readJson(directory / "registries.json");
	for (const auto& [name, entries] : staticRegistries.items()) {
		_staticRegistries[name] = makeRegistry(name, entries);
	}

	json syncedRegistries = readJson(directory / "synced_registries.json");
	for (const auto& [name, entries] : syncedRegistries.items()) {
		_syncedIndex[name] = _syncedRegistries.size();
		_syncedRegistries.push_back(makeRegistry(name, entries));
	}

	json tags = readJson(directory / "tags.json");
	for (const auto& [registryName, registryTagsJson] : tags.items()) {
		const Registry* registry = findRegistry(registryName);
		if (!registry) throw std::runtime_error("tags.json: unknown registry " + registryName);

		RegistryTags registryTags;
		registryTags.registry = registryName;
		for (const auto& [tagName, entries] : registryTagsJson.items()) {
			std::vector<int> ids;
			ids.reserve(entries.size());
			for (const auto& entry : entries) {
				auto it = registry->ids.find(entry.get<std::string>());
				if (it == registry->ids.end()) throw std::runtime_error("tags.json: unknown entry " + entry.get<std::string>() + " in " + registryName);
				ids.push_back(it->second);
			}
			_tagSets[registryName + "#" + tagName] = std::unordered_set<int>(ids.begin(), ids.end());
			registryTags.tags.emplace_back(tagName, std::move(ids));
		}
		_tags.push_back(std::move(registryTags));
	}

	json blocks = readJson(directory / "blocks.json");
	_blockProperties.resize(_staticRegistries.at("minecraft:block").entries.size());
	for (const auto& [block, info] : blocks.items()) {
		_defaultBlockStates[block] = info.at("default").get<int>();
		if (!info.contains("states")) {
			addBlockState(blockStateKey(block, {}), info.at("default").get<int>());
			continue;
		}
		for (const auto& state : info.at("states")) {
			addBlockState(blockStateKey(block, state.at(1).get<std::map<std::string, std::string>>()), state.at(0).get<int>());
		}
	}
	_stateBlocks.resize(_blockStateNames.size());
	for (size_t id = 0; id < _blockStateNames.size(); id++) {
		if (_blockStateNames[id].empty()) throw std::runtime_error("blocks.json: missing block state " + std::to_string(id));
		const std::string& name = _blockStateNames[id];
		_stateBlocks[id]		= getStaticId("minecraft:block", name.substr(0, name.find('[')));
	}

	_blocks.load(
			blocks, [this](const std::string& name) { return getStaticId("minecraft:block", name); },
			_staticRegistries.at("minecraft:block").entries.size(), _blockStateNames.size());

	// Properties once every state is known: checking a shape looks at the block's states
	for (const auto& [block, info] : blocks.items()) {
		for (const auto& [property, value] : info.at("properties").items()) setBlockProperty(block, property, value);
	}

	const Registry& items = _staticRegistries.at("minecraft:item");
	_itemPlacedStates.assign(items.entries.size(), -1);
	_itemProperties.resize(items.entries.size());
	json itemStats = readJson(directory / "items.json");
	for (const auto& [item, properties] : itemStats.items()) {
		for (const auto& [property, value] : properties.items()) setItemProperty(item, property, value);
	}

	json blockItems		  = readJson(directory / "block_items.json");
	for (const auto& [item, block] : blockItems.items()) {
		int itemId = getStaticId("minecraft:item", item);
		if (itemId >= 0) _itemPlacedStates[itemId] = getDefaultBlockState(block.get<std::string>());
	}

	json states = readJson(directory / "block_states.json");
	_stateProperties.resize(_blockStateNames.size());
	for (const auto& [property, values] : states.items()) {
		if (values.size() != _stateProperties.size()) throw std::runtime_error("block_states.json: wrong number of values for " + property);
		for (size_t id = 0; id < values.size(); id++) setStateProperty(_stateProperties[id], property, toNumber(values[id]));
	}

	for (const auto& shape : readJson(directory / "collision_shapes.json")) {
		std::vector<Box>& boxes = _collisionShapes.emplace_back();
		for (const auto& box : shape) {
			boxes.push_back({box.at(0).get<double>(), box.at(1).get<double>(), box.at(2).get<double>(), box.at(3).get<double>(),
							 box.at(4).get<double>(), box.at(5).get<double>()});
		}
	}
	for (const StateProperties& state : _stateProperties) {
		if (state.collisionShape >= _collisionShapes.size() || state.occlusionShape >= _collisionShapes.size()) {
			throw std::runtime_error("block_states.json: unknown shape");
		}
	}

	json outline = readJson(directory / "outline_shapes.json");
	for (const auto& shape : outline.at("shapes")) {
		std::vector<Box>& boxes = _outlineShapes.emplace_back();
		for (const auto& box : shape) {
			boxes.push_back({box.at(0).get<double>(), box.at(1).get<double>(), box.at(2).get<double>(), box.at(3).get<double>(),
							 box.at(4).get<double>(), box.at(5).get<double>()});
		}
	}
	const json& outlineStates = outline.at("states");
	if (outlineStates.size() != _stateProperties.size()) throw std::runtime_error("outline_shapes.json: wrong number of states");
	_outlineShapeOfState.reserve(outlineStates.size());
	for (const auto& index : outlineStates) {
		if (index.get<size_t>() >= _outlineShapes.size()) throw std::runtime_error("outline_shapes.json: unknown shape");
		_outlineShapeOfState.push_back(index.get<uint16_t>());
	}

	json damageTypes = readJson(directory / "damage_types.json");
	for (const auto& [name, type] : damageTypes.items()) _damageExhaustion[name] = type.at("exhaustion").get<float>();

	json dimensions = readJson(directory / "dimensions.json");
	for (const auto& [name, info] : dimensions.items()) {
		Dimension& dimension		  = _dimensions[name];
		dimension.minY				  = info.at("min_y").get<int>();
		dimension.height			  = info.at("height").get<int>();
		dimension.logicalHeight		  = info.value("logical_height", dimension.height);
		dimension.hasSkyLight		  = info.value("has_skylight", true);
		dimension.hasCeiling		  = info.value("has_ceiling", false);
		dimension.ultraWarm			  = info.value("ultrawarm", false);
		dimension.natural			  = info.value("natural", true);
		dimension.bedWorks			  = info.value("bed_works", true);
		dimension.respawnAnchorWorks  = info.value("respawn_anchor_works", false);
		dimension.piglinSafe		  = info.value("piglin_safe", false);
		dimension.hasRaids			  = info.value("has_raids", true);
		dimension.coordinateScale	  = info.value("coordinate_scale", 1.0);
		dimension.ambientLight		  = info.value("ambient_light", 0.0F);
		if (info.contains("fixed_time")) dimension.fixedTime = info.at("fixed_time").get<int64_t>();
		const json& light = info.value("monster_spawn_light_level", json(0));
		if (light.is_number()) {
			dimension.monsterSpawnLightMin = dimension.monsterSpawnLightMax = light.get<int>();
		} else {
			dimension.monsterSpawnLightMin = light.value("min_inclusive", 0);
			dimension.monsterSpawnLightMax = light.value("max_inclusive", 7);
		}
		dimension.monsterSpawnBlockLightLimit = info.value("monster_spawn_block_light_limit", 0);
		dimension.infiniburn				  = info.value("infiniburn", "");
		dimension.effects					  = info.value("effects", "");
	}
	loadEntityTypes(directory / "entity_types.json");
	loadBiomes(directory / "biomes.json");
	if (std::filesystem::exists(directory / "enchantments.json")) _enchantmentData = readJson(directory / "enchantments.json");

	applyOverrides(directory / "overrides.json");
}

void GameData::setBlockProperty(const std::string& block, const std::string& property, const json& value) {
	int blockId = getStaticId("minecraft:block", block);
	if (blockId < 0) throw std::runtime_error("unknown block " + block);

	if (property == "shape") {
		static const std::map<std::string, Shape> SHAPES = {
				{"single", Shape::Single}, {"double_height", Shape::DoubleHeight}, {"double_length", Shape::DoubleLength}};
		auto shape = value.is_string() ? SHAPES.find(value.get<std::string>()) : SHAPES.end();
		if (shape == SHAPES.end()) throw std::runtime_error(block + ": shape must be single, double_height or double_length");
		// The client can only show both parts if the block has states for them
		int state = getDefaultBlockState(block);
		if (shape->second == Shape::DoubleHeight && (withProperty(state, "half", "lower") < 0 || withProperty(state, "half", "upper") < 0)) {
			throw std::runtime_error(block + " can't be double_height: it has no half=lower/upper states");
		}
		if (shape->second == Shape::DoubleLength &&
			(withProperty(state, "part", "foot") < 0 || withProperty(state, "part", "head") < 0 || getProperty(state, "facing").empty())) {
			throw std::runtime_error(block + " can't be double_length: it has no part=foot/head and facing states");
		}
		_blockProperties[blockId].shape = shape->second;
		return;
	}

	if (property == "classes") {
		_blockProperties[blockId].classes = value.get<std::vector<std::string>>();
		return;
	}
	if (property == "block_entity") {
		_blockProperties[blockId].blockEntity = value.get<std::string>();
		return;
	}
	if (property == "dynamic_shape") {
		_blockProperties[blockId].dynamicShape = toNumber(value) != 0;
		return;
	}

	auto field = BLOCK_PROPERTIES.find(property);
	if (field != BLOCK_PROPERTIES.end()) {
		_blockProperties[blockId].*(field->second) = static_cast<float>(toNumber(value));
		return;
	}
	// A state property set on a block applies to all its states
	for (size_t id = 0; id < _stateBlocks.size(); id++) {
		if (_stateBlocks[id] == blockId) setStateProperty(_stateProperties[id], property, toNumber(value));
	}
}

void GameData::applyOverrides(const std::filesystem::path& file) {
	if (!std::filesystem::exists(file)) return;
	json overrides = readJson(file);
	try {
		if (overrides.contains("blocks")) {
			for (const auto& [block, properties] : overrides["blocks"].items()) {
				for (const auto& [property, value] : properties.items()) setBlockProperty(block, property, value);
			}
		}
		if (overrides.contains("items")) {
			for (const auto& [item, properties] : overrides["items"].items()) {
				for (const auto& [property, value] : properties.items()) setItemProperty(item, property, value);
			}
		}
		if (overrides.contains("block_items")) {
			for (const auto& [item, block] : overrides["block_items"].items()) {
				int itemId = getStaticId("minecraft:item", item);
				int state  = getDefaultBlockState(block.get<std::string>());
				if (itemId < 0) throw std::runtime_error("unknown item " + item);
				if (state < 0) throw std::runtime_error("unknown block " + block.get<std::string>());
				_itemPlacedStates[itemId] = state;
			}
		}
	} catch (const std::exception& e) {
		throw std::runtime_error(file.string() + ": " + e.what());
	}
}

void GameData::addBlockState(const std::string& key, int id) {
	_blockStates[key] = id;
	if (static_cast<size_t>(id) >= _blockStateNames.size()) _blockStateNames.resize(id + 1);
	_blockStateNames[id] = key;
}

void GameData::setItemProperty(const std::string& item, const std::string& property, const json& value) {
	int itemId = getStaticId("minecraft:item", item);
	if (itemId < 0) throw std::runtime_error("unknown item " + item);
	ItemProperties& props = _itemProperties[itemId];

	static const std::map<std::string, EquipmentSlot> SLOTS = {
			{"mainhand", EquipmentSlot::MainHand}, {"offhand", EquipmentSlot::OffHand}, {"head", EquipmentSlot::Head},
			{"chest", EquipmentSlot::Chest},	   {"legs", EquipmentSlot::Legs},		{"feet", EquipmentSlot::Feet},
			{"body", EquipmentSlot::Body},		   {"saddle", EquipmentSlot::Saddle}};
	static const std::map<std::string, float ItemProperties::*> STATS = {
			{"attack_damage", &ItemProperties::attackDamage},	  {"attack_speed", &ItemProperties::attackSpeed},
			{"armor", &ItemProperties::armor},					  {"armor_toughness", &ItemProperties::armorToughness},
			{"knockback_resistance", &ItemProperties::knockbackResistance}};

	if (property == "equipment_slot") {
		auto slot = value.is_string() ? SLOTS.find(value.get<std::string>()) : SLOTS.end();
		if (slot == SLOTS.end()) throw std::runtime_error(item + ": unknown equipment_slot " + value.dump());
		props.equipmentSlot = slot->second;
	} else if (property == "max_stack_size") {
		int size = static_cast<int>(toNumber(value));
		if (size < 1 || size > 99) throw std::runtime_error(item + ": max_stack_size must be between 1 and 99");
		props.maxStackSize = size;
	} else if (auto stat = STATS.find(property); stat != STATS.end()) {
		props.*(stat->second) = static_cast<float>(toNumber(value));
	} else if (property == "crafting_remainder") {
		props.craftingRemainder = value.get<std::string>();
	} else if (property == "food") {
		props.food = Food{value.at("nutrition").get<int>(), value.at("saturation").get<float>(), value.value("can_always_eat", false)};
	} else if (property == "consumable") {
		static const std::map<std::string, UseAnimation> ANIMATIONS = {
				{"none", UseAnimation::None},	{"eat", UseAnimation::Eat},			  {"drink", UseAnimation::Drink},
				{"block", UseAnimation::Block}, {"bow", UseAnimation::Bow},			  {"trident", UseAnimation::Trident},
				{"crossbow", UseAnimation::Crossbow}, {"spyglass", UseAnimation::Spyglass}, {"toot_horn", UseAnimation::TootHorn},
				{"brush", UseAnimation::Brush}, {"bundle", UseAnimation::Bundle}};
		// Absent fields have Consumable.CODEC's defaults
		Consumable consumable;
		consumable.consumeSeconds	   = value.value("consume_seconds", consumable.consumeSeconds);
		auto animation				   = ANIMATIONS.find(value.value("animation", std::string("eat")));
		consumable.animation		   = animation != ANIMATIONS.end() ? animation->second : UseAnimation::None;
		consumable.sound			   = value.value("sound", consumable.sound);
		consumable.hasConsumeParticles = value.value("has_consume_particles", true);
		if (value.contains("on_consume_effects")) {
			for (const auto& effect : value.at("on_consume_effects")) {
				consumable.onConsumeEffects.push_back({effect.at("type").get<std::string>(),
													   effect.contains("sound") && effect.at("sound").is_string() ? effect.at("sound").get<std::string>() : ""});
			}
		}
		props.consumable = std::move(consumable);
	} else if (property == "use_remainder") {
		props.useRemainder		= value.at("id").get<std::string>();
		props.useRemainderCount = value.value("count", 1);
	} else if (property == "swappable") {
		props.swappable = value.get<bool>();
	} else if (property == "equip_sound") {
		props.equipSound = value.is_string() ? value.get<std::string>() : "";
	} else if (property == "blocks_attacks") {
		props.blocksAttacks = value.get<bool>();
	} else if (property == "fire_resistant") {
		props.fireResistant = toNumber(value) != 0;
	} else if (property == "can_destroy_blocks_in_creative") {
		props.canDestroyBlocksInCreative = value.get<bool>();
	} else if (property == "tool_rules") {
		props.toolRules.clear();
		for (const auto& rule : value) {
			ItemProperties::ToolRule parsed;
			parsed.blocks.assign(getBlockCount(), false);
			std::vector<std::string> names;
			if (rule.at("blocks").is_array()) {
				names = rule.at("blocks").get<std::vector<std::string>>();
			} else {
				names.push_back(rule.at("blocks").get<std::string>());
			}
			for (const std::string& name : names) {
				if (name.rfind('#', 0) == 0) {
					std::vector<bool> tag = blockTag(name.substr(1));
					for (size_t i = 0; i < tag.size(); i++) parsed.blocks[i] = parsed.blocks[i] || tag[i];
				} else if (int block = getStaticId("minecraft:block", name); block >= 0) {
					parsed.blocks[block] = true;
				}
			}
			if (rule.contains("correct_for_drops")) parsed.correctForDrops = rule.at("correct_for_drops").get<bool>() ? 1 : 0;
			props.toolRules.push_back(std::move(parsed));
		}
	} else if (property == "attribute_modifiers") {
		props.attributeModifiers.clear();
		for (const json& entry : value) {
			int attribute = getStaticId("minecraft:attribute", entry.at("type").get<std::string>());
			if (attribute < 0) continue;
			std::string operation = entry.value("operation", "add_value");
			props.attributeModifiers.push_back({attribute, entry.value("amount", 0.0), entry.value("id", ""),
												operation == "add_multiplied_total" ? 2 : operation == "add_multiplied_base" ? 1 : 0, entry.value("slot", "any")});
		}
	} else if (property == "max_damage") {
		props.maxDamage = static_cast<int>(toNumber(value));
	} else if (property == "enchantable") {
		props.enchantability = value.value("value", 0);
	} else if (property == "repairable") {
		const json& items = value.at("items");
		props.repairItems = items.is_string() ? items.get<std::string>() : items.dump();
	} else if (property == "weapon") {
		props.isWeapon					= true;
		props.weaponDamagePerAttack		= value.value("item_damage_per_attack", 1);
		props.disableBlockingForSeconds = value.value("disable_blocking_for_seconds", 0.0F);
	} else if (property == "use_cooldown") {
		props.useCooldownSeconds = value.value("seconds", 0.0F);
		props.useCooldownGroup	 = value.value("cooldown_group", "");
	} else if (property == "glider") {
		props.glider = true;
	} else if (property == "tool") {
		props.isTool			 = true;
		props.defaultMiningSpeed = value.value("default_mining_speed", 1.0F);
		props.toolDamagePerBlock = value.value("damage_per_block", 1);
		// The rule speeds, in the order of tool_rules (read first)
		const json& rules = value.value("rules", json::array());
		for (size_t i = 0; i < rules.size() && i < props.toolRules.size(); i++) {
			if (rules[i].contains("speed")) props.toolRules[i].speed = rules[i].at("speed").get<float>();
		}
	} else if (property == "potion_contents" || property == "charged_projectiles" || property == "fireworks" || property == "death_protection") {
		props.rawComponents[property] = value;
	} else {
		throw std::runtime_error("unknown item property \"" + property + "\"");
	}
}

bool GameData::isCorrectToolForDrops(int itemId, int stateId) const {
	const ItemProperties* item = getItemProperties(itemId);
	if (!item) return false;
	int block = getBlockOfState(stateId);
	for (const ItemProperties::ToolRule& rule : item->toolRules) {
		if (rule.correctForDrops >= 0 && rule.blocks[block]) return rule.correctForDrops == 1;
	}
	return false;
}

float GameData::getDamageExhaustion(const std::string& type) const {
	auto it = _damageExhaustion.find(type);
	return it != _damageExhaustion.end() ? it->second : 0.0F;
}

void GameData::loadEntityTypes(const std::filesystem::path& file) {
	json data = readJson(file);
	_attributes.resize(_staticRegistries.at("minecraft:attribute").entries.size());
	for (const auto& [name, info] : data.at("attributes").items()) {
		int id = getStaticId("minecraft:attribute", name);
		if (id < 0) throw std::runtime_error("entity_types.json: unknown attribute " + name);
		AttributeInfo& attribute = _attributes[id];
		attribute.defaultValue	 = info.at("default").get<double>();
		attribute.minValue		 = info.value("min", attribute.minValue);
		attribute.maxValue		 = info.value("max", attribute.maxValue);
		attribute.syncable		 = info.value("syncable", false);
	}
	static const std::map<std::string, MobCategory> CATEGORIES = {
			{"monster", MobCategory::Monster},
			{"creature", MobCategory::Creature},
			{"ambient", MobCategory::Ambient},
			{"axolotls", MobCategory::Axolotls},
			{"underground_water_creature", MobCategory::UndergroundWaterCreature},
			{"water_creature", MobCategory::WaterCreature},
			{"water_ambient", MobCategory::WaterAmbient},
			{"misc", MobCategory::Misc}};
	_entityTypes.resize(_staticRegistries.at("minecraft:entity_type").entries.size());
	for (const auto& [name, info] : data.at("types").items()) {
		int id = getStaticId("minecraft:entity_type", name);
		if (id < 0) throw std::runtime_error("entity_types.json: unknown entity type " + name);
		EntityTypeInfo& type	= _entityTypes[id];
		type.width				= info.at("width").get<float>();
		type.height				= info.at("height").get<float>();
		type.eyeHeight			= info.at("eye_height").get<float>();
		type.fixedSize			= info.value("fixed", false);
		type.trackingRange		= info.value("tracking_range", 5);
		type.updateInterval		= info.value("update_interval", 3);
		type.trackDeltas		= info.value("track_deltas", true);
		type.fireImmune			= info.value("fire_immune", false);
		type.summonable			= info.value("summonable", true);
		type.serializable		= info.value("serializable", true);
		type.allowedInPeaceful	= info.value("allowed_in_peaceful", true);
		auto category			= CATEGORIES.find(info.value("category", "misc"));
		type.category			= category != CATEGORIES.end() ? category->second : MobCategory::Misc;
		type.lootTable			= info.value("loot_table", "");
		type.classes			= info.value("classes", std::vector<std::string>());
		type.living				= type.is("LivingEntity");
		type.mob				= type.is("Mob");
		if (info.contains("attributes")) {
			type.attributes.assign(_attributes.size(), std::nan(""));
			for (const auto& [attribute, value] : info.at("attributes").items()) {
				int attributeId = getStaticId("minecraft:attribute", attribute);
				if (attributeId >= 0) type.attributes[attributeId] = value.get<double>();
			}
		}
		if (info.contains("spawn_egg")) {
			type.spawnEgg = getStaticId("minecraft:item", info.at("spawn_egg").get<std::string>());
			if (type.spawnEgg >= 0) _spawnEggTypes[type.spawnEgg] = id;
		}
	}
}

bool GameData::EntityTypeInfo::is(const std::string& javaClass) const { return std::find(classes.begin(), classes.end(), javaClass) != classes.end(); }

const GameData::EntityTypeInfo* GameData::getEntityType(int typeId) const {
	if (typeId < 0 || static_cast<size_t>(typeId) >= _entityTypes.size()) return nullptr;
	return &_entityTypes[typeId];
}

int GameData::getSpawnEggType(int itemId) const {
	auto it = _spawnEggTypes.find(itemId);
	return it == _spawnEggTypes.end() ? -1 : it->second;
}

const GameData::ItemProperties* GameData::getItemProperties(int itemId) const {
	if (itemId < 0 || static_cast<size_t>(itemId) >= _itemProperties.size()) return nullptr;
	return &_itemProperties[itemId];
}

int GameData::getPlacedBlockState(int itemId) const {
	if (itemId < 0 || static_cast<size_t>(itemId) >= _itemPlacedStates.size()) return -1;
	return _itemPlacedStates[itemId];
}

bool GameData::isInTag(const std::string& registry, const std::string& tag, int entryId) const {
	auto it = _tagSets.find(registry + "#" + tag);
	return it != _tagSets.end() && it->second.count(entryId);
}

int GameData::withProperty(int stateId, const std::string& property, const std::string& value) const {
	// "minecraft:oak_log[axis=y]" -> replace the value of "axis"
	std::string name  = _blockStateNames.at(stateId);
	size_t		start = name.find("[" + property + "=");
	if (start == std::string::npos) start = name.find("," + property + "=");
	if (start == std::string::npos) return -1;
	size_t valueStart = start + property.size() + 2;
	size_t valueEnd	  = name.find_first_of(",]", valueStart);
	return getBlockStateFromName(name.substr(0, valueStart) + value + name.substr(valueEnd));
}

std::string GameData::getProperty(int stateId, const std::string& property) const {
	const std::string& name	 = _blockStateNames.at(stateId);
	size_t			   start = name.find("[" + property + "=");
	if (start == std::string::npos) start = name.find("," + property + "=");
	if (start == std::string::npos) return "";
	size_t valueStart = start + property.size() + 2;
	return name.substr(valueStart, name.find_first_of(",]", valueStart) - valueStart);
}

int GameData::getBlockStateFromName(const std::string& name) const {
	auto it = _blockStates.find(name);
	return it == _blockStates.end() ? -1 : it->second;
}

void GameData::loadBiomes(const std::filesystem::path& file) {
	const Registry* registry = nullptr;
	for (const Registry& synced : _syncedRegistries) {
		if (synced.name == "minecraft:worldgen/biome") registry = &synced;
	}
	if (!registry) return;
	_biomes.assign(registry->entries.size(), Biome{});
	if (!std::filesystem::exists(file)) return;
	static const std::map<std::string, MobCategory> CATEGORIES = {
			{"monster", MobCategory::Monster},	 {"creature", MobCategory::Creature},
			{"ambient", MobCategory::Ambient},	 {"axolotls", MobCategory::Axolotls},
			{"underground_water_creature", MobCategory::UndergroundWaterCreature},
			{"water_creature", MobCategory::WaterCreature}, {"water_ambient", MobCategory::WaterAmbient},
			{"misc", MobCategory::Misc}};
	json biomes = readJson(file);
	for (const auto& [name, info] : biomes.items()) {
		auto id = registry->ids.find(name);
		if (id == registry->ids.end()) continue;
		Biome& biome				   = _biomes[id->second];
		biome.temperature			   = info.value("temperature", 0.8F);
		biome.downfall				   = info.value("downfall", 0.4F);
		biome.hasPrecipitation		   = info.value("has_precipitation", true);
		biome.frozenModifier		   = info.value("temperature_modifier", "none") == "frozen";
		biome.creatureSpawnProbability = info.value("creature_spawn_probability", 0.1F);
		const json spawners = info.value("spawners", json::object());
		for (const auto& [category, list] : spawners.items()) {
			auto found = CATEGORIES.find(category);
			if (found == CATEGORIES.end()) continue;
			for (const auto& spawner : list) {
				int type = getStaticId("minecraft:entity_type", spawner.at("type").get<std::string>());
				if (type < 0) continue;
				biome.spawners[static_cast<int>(found->second)].push_back(
						{type, spawner.value("weight", 1), spawner.value("minCount", 1), spawner.value("maxCount", 1)});
			}
		}
		const json costs = info.value("spawn_costs", json::object());
		for (const auto& [type, cost] : costs.items()) {
			int typeId = getStaticId("minecraft:entity_type", type);
			if (typeId >= 0) biome.spawnCosts[typeId] = {cost.value("energy_budget", 0.0), cost.value("charge", 0.0)};
		}
	}
}

const GameData::Dimension* GameData::getDimension(const std::string& name) const {
	auto it = _dimensions.find(name);
	return it == _dimensions.end() ? nullptr : &it->second;
}

const GameData::Registry* GameData::findRegistry(const std::string& registry) const {
	auto synced = _syncedIndex.find(registry);
	if (synced != _syncedIndex.end()) return &_syncedRegistries[synced->second];
	auto staticIt = _staticRegistries.find(registry);
	if (staticIt != _staticRegistries.end()) return &staticIt->second;
	return nullptr;
}

int GameData::getStaticId(const std::string& registry, const std::string& entry) const {
	auto registryIt = _staticRegistries.find(registry);
	if (registryIt == _staticRegistries.end()) return -1;
	auto entryIt = registryIt->second.ids.find(entry);
	return entryIt == registryIt->second.ids.end() ? -1 : entryIt->second;
}

std::vector<bool> GameData::blockTag(const std::string& tag) const {
	std::vector<bool> members(getBlockCount(), false);
	auto			  it = _tagSets.find("minecraft:block#" + tag);
	if (it == _tagSets.end()) throw std::runtime_error("unknown block tag " + tag);
	for (int id : it->second) members[id] = true;
	return members;
}

bool GameData::isInstanceOf(int blockId, const std::string& javaClass) const {
	const std::vector<std::string>& classes = _blockProperties.at(blockId).classes;
	return std::find(classes.begin(), classes.end(), javaClass) != classes.end();
}

const std::string& GameData::getStaticName(const std::string& registry, int id) const {
	static const std::string none;
	auto					 registryIt = _staticRegistries.find(registry);
	if (registryIt == _staticRegistries.end() || id < 0 || id >= static_cast<int>(registryIt->second.entries.size())) return none;
	return registryIt->second.entries[id];
}

int GameData::getSyncedId(const std::string& registry, const std::string& entry) const {
	auto registryIt = _syncedIndex.find(registry);
	if (registryIt == _syncedIndex.end()) return -1;
	const Registry& synced	= _syncedRegistries[registryIt->second];
	auto			entryIt = synced.ids.find(entry);
	return entryIt == synced.ids.end() ? -1 : entryIt->second;
}

int GameData::getDefaultBlockState(const std::string& block) const {
	auto it = _defaultBlockStates.find(block);
	return it == _defaultBlockStates.end() ? -1 : it->second;
}

int GameData::getBlockState(const std::string& block, const std::map<std::string, std::string>& properties) const {
	auto it = _blockStates.find(blockStateKey(block, properties));
	return it == _blockStates.end() ? -1 : it->second;
}

// "minecraft:oak_log[axis=y]", or "minecraft:stone" without properties.
// std::map keeps properties sorted, so the key doesn't depend on their order
std::string GameData::blockStateKey(const std::string& block, const std::map<std::string, std::string>& properties) {
	if (properties.empty()) return block;
	std::string key = block + "[";
	bool		first = true;
	for (const auto& [name, value] : properties) {
		if (!first) key += ",";
		key += name + "=" + value;
		first = false;
	}
	return key + "]";
}
