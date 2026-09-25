#ifndef BLOCK_REGISTRY_HPP
#define BLOCK_REGISTRY_HPP

#include "lib/json.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Block states compiled for the game logic: reading or changing a property is arithmetic, not a string lookup.
//
// Like vanilla's StateDefinition, the states of a block are the cartesian product of its properties, sorted by
// name, the last one varying fastest: state = first state + sum(value index * stride). So changing a property is
// adding (new index - old index) * stride. The layout is checked against every state of blocks.json when loading.
//
// Property and value names are interned once (property("facing"), value("north")): game code keeps these ids.
class BlockRegistry {
  public:
	// blocks: blocks.json. blockIdOf: minecraft:block registry id of a block name
	void load(const nlohmann::json& blocks, const std::function<int(const std::string&)>& blockIdOf, size_t blockCount, size_t stateCount);

	// Interned names, -1 if no block uses them
	int				   property(const std::string& name) const;
	int				   value(const std::string& name) const;
	const std::string& propertyName(int property) const { return _propertyNames.at(property); }
	const std::string& valueName(int value) const { return _valueNames.at(value); }
	// Integer values ("0".."25") as numbers, -1 for the others
	int valueAsInt(int value) const { return _valueInts[value]; }

	int	 blockOf(int state) const { return _stateBlocks[state]; }
	int	 defaultState(int block) const { return _blocks[block].defaultState; }
	bool isAir(int state) const { return _stateFlags[state] & FLAG_AIR; }
	bool is(int state, int block) const { return _stateBlocks[state] == block; }
	size_t stateCount() const { return _stateBlocks.size(); }

	// Value id of a property, -1 if the block doesn't have it
	int	 get(int state, int property) const;
	int	 getInt(int state, int property) const;
	bool getBool(int state, int property) const;
	bool has(int state, int property) const { return findLayout(_blocks[_stateBlocks[state]], property) != nullptr; }
	// Same state with one property changed, -1 if the block doesn't have it or the value isn't allowed
	int with(int state, int property, int value) const;
	int withInt(int state, int property, int number) const;
	int withBool(int state, int property, bool flag) const;
	// Next value of the property, wrapping (vanilla's cycle)
	int cycle(int state, int property) const;

  private:
	static constexpr uint8_t FLAG_AIR = 1;

	struct PropertyLayout {
		int					  property;
		uint32_t			  stride;
		std::vector<uint16_t> values; // Value ids, in the block's order
	};
	struct BlockInfo {
		int							firstState	 = 0;
		int							defaultState = 0;
		std::vector<PropertyLayout> properties;
	};

	std::vector<BlockInfo>				 _blocks;
	std::vector<uint16_t>				 _stateBlocks;
	std::vector<uint8_t>				 _stateFlags;
	std::vector<std::string>			 _propertyNames;
	std::vector<std::string>			 _valueNames;
	std::vector<int>					 _valueInts;
	std::vector<int>					 _intValues; // Value id of each number, -1 if no property has it
	std::unordered_map<std::string, int> _propertyIds;
	std::unordered_map<std::string, int> _valueIds;
	int									 _true = -1, _false = -1;

	int					  internProperty(const std::string& name);
	int					  internValue(const std::string& name);
	static const PropertyLayout* findLayout(const BlockInfo& block, int property);
	static int			  indexOf(const PropertyLayout& layout, int value);
};

#endif
