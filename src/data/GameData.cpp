#include "data/GameData.hpp"

#include "lib/json.hpp"

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

	json dimensions = readJson(directory / "dimensions.json");
	for (const auto& [name, info] : dimensions.items()) {
		_dimensions[name] = {info.at("min_y").get<int>(), info.at("height").get<int>()};
	}

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
	} else {
		throw std::runtime_error("unknown item property \"" + property + "\"");
	}
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
