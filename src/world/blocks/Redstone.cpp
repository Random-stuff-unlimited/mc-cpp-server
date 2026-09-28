#include "world/blocks/Redstone.hpp"

#include "world/PlaceContext.hpp"

#include <cmath>

// The redstone components' shared bits: the property and value ids, the rotations, and the classes with no behavior
// of their own (PoweredBlock, FacingPlacement, AnalogOutputBlock). The components themselves are split by family:
// RedstoneWire.cpp, RedstoneTorch.cpp, RedstoneDiode.cpp, RedstoneInput.cpp, RedstoneOutput.cpp, RedstoneDoors.cpp

Direction clockWise(Direction direction) {
	switch (direction) {
	case Direction::North:
		return Direction::East;
	case Direction::East:
		return Direction::South;
	case Direction::South:
		return Direction::West;
	case Direction::West:
		return Direction::North;
	default:
		return direction;
	}
}

Direction counterClockWise(Direction direction) {
	switch (direction) {
	case Direction::North:
		return Direction::West;
	case Direction::West:
		return Direction::South;
	case Direction::South:
		return Direction::East;
	case Direction::East:
		return Direction::North;
	default:
		return direction;
	}
}

RedstoneIds::RedstoneIds(const BlockContext& context) {
	const BlockRegistry& b = context.blocks;
	power				   = b.property("power");
	powered				   = b.property("powered");
	lit					   = b.property("lit");
	face				   = b.property("face");
	delay				   = b.property("delay");
	locked				   = b.property("locked");
	mode				   = b.property("mode");
	note				   = b.property("note");
	instrument			   = b.property("instrument");
	open				   = b.property("open");
	hinge				   = b.property("hinge");
	inWall				   = b.property("in_wall");
	sides[0]			   = b.property("north");
	sides[1]			   = b.property("east");
	sides[2]			   = b.property("south");
	sides[3]			   = b.property("west");
	none				   = b.value("none");
	side				   = b.value("side");
	up					   = b.value("up");
	floor				   = b.value("floor");
	wall				   = b.value("wall");
	ceiling				   = b.value("ceiling");
	compare				   = b.value("compare");
	subtract			   = b.value("subtract");
	wire				   = context.block("minecraft:redstone_wire");
	repeater			   = context.block("minecraft:repeater");
	comparator			   = context.block("minecraft:comparator");
	observer			   = context.block("minecraft:observer");
	redstoneBlock		   = context.block("minecraft:redstone_block");
	hopper				   = context.block("minecraft:hopper");
	air					   = context.defaultState("minecraft:air");
}

int RedstoneIds::sideIndex(Direction direction) {
	switch (direction) {
	case Direction::North:
		return 0;
	case Direction::East:
		return 1;
	case Direction::South:
		return 2;
	default:
		return 3;
	}
}

// PoweredBlock (block of redstone) and FacingPlacement are declared with their bodies in the header.

// The facing rules of the simple directional blocks (getStateForPlacement of DispenserBlock, BarrelBlock,
// AbstractFurnaceBlock, AnvilBlock, HopperBlock, ShulkerBoxBlock...)
int FacingPlacement::getStateForPlacement(Level&, const PlaceContext& context) const {
	Direction facing;
	switch (_rule) {
	case Rule::NearestOpposite:
		facing = Directions::opposite(context.nearestLookingDirection());
		break;
	case Rule::HorizontalOpposite:
		facing = Directions::opposite(context.horizontalDirection());
		break;
	case Rule::HorizontalClockwise:
		facing = clockWise(context.horizontalDirection());
		break;
	case Rule::ClickedFace:
		facing = context.clickedFace;
		break;
	case Rule::Hopper: {
		Direction into = Directions::opposite(context.clickedFace);
		facing		   = into == Direction::Up || into == Direction::Down ? Direction::Down : into;
		break;
	}
	}
	const BlockRegistry& blocks = _context->blocks;
	int					 state	= blocks.with(blocks.defaultState(context.block), _context->facing, _context->directionValue(facing));
	return state >= 0 ? state : GENERIC_PLACEMENT;
}

// Blocks comparators read from their state alone (cake, composter, cauldrons, end portal frame, respawn anchor, beehive)
AnalogOutputBlock::AnalogOutputBlock(std::shared_ptr<const BlockContext> context, Kind kind) : _context(std::move(context)), _kind(kind) {
	const char* names[] = {"bites", "", "level", "", "eye", "charges", "honey_level"};
	_property			= names[static_cast<int>(kind)][0] ? _context->blocks.property(names[static_cast<int>(kind)]) : -1;
}

int AnalogOutputBlock::getAnalogOutputSignal(Level&, const BlockPos&, int state, Direction) const {
	const BlockRegistry& b = _context->blocks;
	switch (_kind) {
	case Kind::Cake:
		return (7 - b.getInt(state, _property)) * 2;
	case Kind::CandleCake:
		return 14;
	case Kind::Level:
	case Kind::HoneyLevel:
		return b.getInt(state, _property);
	case Kind::LavaCauldron:
		return 3;
	case Kind::EndPortalFrame:
		return b.getBool(state, _property) ? 15 : 0;
	case Kind::RespawnAnchor:
		return static_cast<int>(std::floor(b.getInt(state, _property) / 4.0F * 15));
	}
	return 0;
}