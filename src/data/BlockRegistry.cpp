#include "data/BlockRegistry.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

void BlockRegistry::load(const nlohmann::json& blocks, const std::function<int(const std::string&)>& blockIdOf, size_t blockCount,
						 size_t stateCount) {
	_blocks.assign(blockCount, BlockInfo());
	_stateBlocks.assign(stateCount, 0);
	_stateFlags.assign(stateCount, 0);

	for (const auto& [name, info] : blocks.items()) {
		int block = blockIdOf(name);
		if (block < 0 || block >= static_cast<int>(blockCount)) throw std::runtime_error("blocks.json: unknown block " + name);
		BlockInfo& compiled	  = _blocks[block];
		compiled.defaultState = info.at("default").get<int>();

		// States with their property values, by id
		std::vector<std::pair<int, std::map<std::string, std::string>>> states;
		if (info.contains("states")) {
			for (const auto& state : info.at("states")) states.emplace_back(state.at(0).get<int>(), state.at(1).get<std::map<std::string, std::string>>());
		} else {
			states.emplace_back(compiled.defaultState, std::map<std::string, std::string>());
		}
		std::sort(states.begin(), states.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
		compiled.firstState = states.front().first;

		// Properties sorted by name (std::map order), the last one varying fastest; values in order of appearance
		const auto& first = states.front().second;
		uint32_t	stride = 1;
		for (auto it = first.rbegin(); it != first.rend(); ++it) {
			PropertyLayout layout{internProperty(it->first), stride, {}};
			for (size_t i = 0; i < states.size(); i += stride) {
				int value = internValue(states[i].second.at(it->first));
				if (std::find(layout.values.begin(), layout.values.end(), value) != layout.values.end()) break;
				layout.values.push_back(static_cast<uint16_t>(value));
			}
			stride *= static_cast<uint32_t>(layout.values.size());
			compiled.properties.insert(compiled.properties.begin(), std::move(layout));
		}
		if (stride != states.size()) throw std::runtime_error("blocks.json: unexpected state layout for " + name);

		// Every state must be where the arithmetic puts it
		for (const auto& [id, values] : states) {
			int expected = compiled.firstState;
			for (const PropertyLayout& layout : compiled.properties) {
				int index = indexOf(layout, _valueIds.at(values.at(_propertyNames[layout.property])));
				if (index < 0) throw std::runtime_error("blocks.json: unexpected value in " + name);
				expected += index * static_cast<int>(layout.stride);
			}
			if (expected != id) throw std::runtime_error("blocks.json: state " + std::to_string(id) + " of " + name + " isn't where expected");
			if (id < 0 || id >= static_cast<int>(stateCount)) throw std::runtime_error("blocks.json: state id out of range");
			_stateBlocks[id] = static_cast<uint16_t>(block);
			if (name == "minecraft:air" || name == "minecraft:cave_air" || name == "minecraft:void_air") _stateFlags[id] |= FLAG_AIR;
		}
	}
	_true  = value("true");
	_false = value("false");
}

int BlockRegistry::internProperty(const std::string& name) {
	auto [it, added] = _propertyIds.emplace(name, static_cast<int>(_propertyNames.size()));
	if (added) _propertyNames.push_back(name);
	return it->second;
}

int BlockRegistry::internValue(const std::string& name) {
	auto [it, added] = _valueIds.emplace(name, static_cast<int>(_valueNames.size()));
	if (added) {
		_valueNames.push_back(name);
		bool numeric = !name.empty() && name.size() < 4 && std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; });
		_valueInts.push_back(numeric ? std::stoi(name) : -1);
		if (numeric) {
			int number = _valueInts.back();
			if (static_cast<int>(_intValues.size()) <= number) _intValues.resize(number + 1, -1);
			_intValues[number] = it->second;
		}
	}
	return it->second;
}

int BlockRegistry::property(const std::string& name) const {
	auto it = _propertyIds.find(name);
	return it == _propertyIds.end() ? -1 : it->second;
}

int BlockRegistry::value(const std::string& name) const {
	auto it = _valueIds.find(name);
	return it == _valueIds.end() ? -1 : it->second;
}

const BlockRegistry::PropertyLayout* BlockRegistry::findLayout(const BlockInfo& block, int property) {
	for (const PropertyLayout& layout : block.properties) {
		if (layout.property == property) return &layout;
	}
	return nullptr;
}

int BlockRegistry::indexOf(const PropertyLayout& layout, int value) {
	for (size_t i = 0; i < layout.values.size(); i++) {
		if (layout.values[i] == value) return static_cast<int>(i);
	}
	return -1;
}

int BlockRegistry::get(int state, int property) const {
	const BlockInfo&	  block	 = _blocks[_stateBlocks[state]];
	const PropertyLayout* layout = findLayout(block, property);
	if (!layout) return -1;
	return layout->values[(static_cast<uint32_t>(state - block.firstState) / layout->stride) % layout->values.size()];
}

int BlockRegistry::getInt(int state, int property) const {
	int value = get(state, property);
	return value < 0 ? -1 : _valueInts[value];
}

bool BlockRegistry::getBool(int state, int property) const { return get(state, property) == _true; }

int BlockRegistry::with(int state, int property, int value) const {
	const BlockInfo&	  block	 = _blocks[_stateBlocks[state]];
	const PropertyLayout* layout = findLayout(block, property);
	if (!layout) return -1;
	int newIndex = indexOf(*layout, value);
	if (newIndex < 0) return -1;
	int oldIndex = static_cast<int>((static_cast<uint32_t>(state - block.firstState) / layout->stride) % layout->values.size());
	return state + (newIndex - oldIndex) * static_cast<int>(layout->stride);
}

int BlockRegistry::withInt(int state, int property, int number) const {
	if (number < 0 || number >= static_cast<int>(_intValues.size()) || _intValues[number] < 0) return -1;
	return with(state, property, _intValues[number]);
}

int BlockRegistry::withBool(int state, int property, bool flag) const { return with(state, property, flag ? _true : _false); }

int BlockRegistry::cycle(int state, int property) const {
	const BlockInfo&	  block	 = _blocks[_stateBlocks[state]];
	const PropertyLayout* layout = findLayout(block, property);
	if (!layout) return -1;
	int count	 = static_cast<int>(layout->values.size());
	int oldIndex = static_cast<int>((static_cast<uint32_t>(state - block.firstState) / layout->stride) % count);
	int newIndex = (oldIndex + 1) % count;
	return state + (newIndex - oldIndex) * static_cast<int>(layout->stride);
}
