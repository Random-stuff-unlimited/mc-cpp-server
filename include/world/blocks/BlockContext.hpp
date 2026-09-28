#ifndef BLOCK_CONTEXT_HPP
#define BLOCK_CONTEXT_HPP

#include "data/GameData.hpp"
#include "world/BlockPos.hpp"

#include <string>
#include <vector>

class Level;

// What the block behaviors share: property and value ids, looked up once, and vanilla's common block questions
struct BlockContext {
	const GameData&		 data;
	const BlockRegistry& blocks;

	// Properties
	int facing, half, part, occupied, hanging, layers, age, waterlogged, moisture;
	// Values
	int lower, upper, foot, head;

	explicit BlockContext(const GameData& gameData);

	int block(const std::string& name) const { return data.getStaticId("minecraft:block", name); }
	int defaultState(const std::string& name) const { return data.getDefaultBlockState(name); }
	bool is(int state, int block) const { return blocks.blockOf(state) == block; }
	bool isAir(int state) const { return blocks.isAir(state); }
	// A block tag as a table by block id
	std::vector<bool> tag(const std::string& name) const { return data.blockTag(name); }
	bool			  inTag(const std::vector<bool>& tag, int state) const { return tag[blocks.blockOf(state)]; }

	// Direction stored in a property (facing...), and the value id of a direction
	Direction direction(int state, int property) const;
	int		  directionValue(Direction direction) const { return _directionValues[static_cast<int>(direction)]; }
	const GameData::StateProperties& properties(int state) const { return data.getStateProperties(state); }
	// BlockState.isFaceSturdy: the side can support a block
	bool isFaceSturdy(int state, Direction side, GameData::SupportType type = GameData::SupportType::Full) const {
		return data.isFaceSturdy(state, static_cast<int>(side), type);
	}
	// Block.canSupportCenter: the block at pos can hold something small (torch...) on this side
	bool canSupportCenter(Level& level, const BlockPos& pos, Direction side) const;

  private:
	int _directionValues[6]; // Value id of each direction's name
};

#endif
