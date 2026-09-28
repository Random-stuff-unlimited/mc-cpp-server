#include "world/blocks/BlockContext.hpp"

#include "world/Level.hpp"

BlockContext::BlockContext(const GameData& gameData) : data(gameData), blocks(gameData.getBlocks()) {
	facing		= blocks.property("facing");
	half		= blocks.property("half");
	part		= blocks.property("part");
	occupied	= blocks.property("occupied");
	hanging		= blocks.property("hanging");
	layers		= blocks.property("layers");
	age			= blocks.property("age");
	waterlogged = blocks.property("waterlogged");
	moisture	= blocks.property("moisture");
	lower		= blocks.value("lower");
	upper		= blocks.value("upper");
	foot		= blocks.value("foot");
	head		= blocks.value("head");
	const char* names[6] = {"down", "up", "north", "south", "west", "east"};
	for (int i = 0; i < 6; i++) _directionValues[i] = blocks.value(names[i]);
}

Direction BlockContext::direction(int state, int property) const {
	int value = blocks.get(state, property);
	for (int i = 0; i < 6; i++) {
		if (_directionValues[i] == value) return static_cast<Direction>(i);
	}
	return Direction::North;
}

bool BlockContext::canSupportCenter(Level& level, const BlockPos& pos, Direction side) const {
	return isFaceSturdy(level.getBlockState(pos), side, GameData::SupportType::Center);
}
