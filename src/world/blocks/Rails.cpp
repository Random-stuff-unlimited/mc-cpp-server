#include "world/blocks/Rails.hpp"

#include "world/Level.hpp"
#include "world/entity/Vehicles.hpp"

#include <array>

namespace {
	struct Step {
		int dx, dz;
	};
	// The two horizontal directions a shape runs along (its ends)
	std::array<Step, 2> ends(const std::string& shape) {
		if (shape == "east_west" || shape == "ascending_east" || shape == "ascending_west") return {{{-1, 0}, {1, 0}}};
		if (shape == "south_east") return {{{0, 1}, {1, 0}}};
		if (shape == "south_west") return {{{0, 1}, {-1, 0}}};
		if (shape == "north_west") return {{{0, -1}, {-1, 0}}};
		if (shape == "north_east") return {{{0, -1}, {1, 0}}};
		return {{{0, -1}, {0, 1}}}; // north_south, ascending_north, ascending_south
	}
	std::string blockName(Level& level, int state) { return level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state)); }
} // namespace

bool RailBehavior::isRail(Level& level, const BlockPos& pos) {
	return level.gameData().isInTag("minecraft:block", "minecraft:rails", level.blocks().blockOf(level.getBlockState(pos)));
}

std::string RailBehavior::shape(Level& level, int state) { return level.gameData().getProperty(state, "shape"); }

bool RailBehavior::canSurvive(Level& level, const BlockPos& pos, int) const {
	// BaseRailBlock.canSurvive: a block under it that can hold it (canSupportRigidBlock)
	return level.gameData().isFaceSturdy(level.getBlockState(pos.below()), static_cast<int>(Direction::Up), GameData::SupportType::Rigid);
}

void RailBehavior::connect(Level& level, const BlockPos& pos, bool neighborsToo) const {
	int state = level.getBlockState(pos);
	// The rails around: next to it, a block up (it slopes up to them) or a block down (they slope up to it)
	bool n = false, s = false, e = false, w = false, upN = false, upS = false, upE = false, upW = false;
	struct Dir {
		int	   dx, dz;
		bool  *flag, *up;
	};
	Dir dirs[] = {{0, -1, &n, &upN}, {0, 1, &s, &upS}, {1, 0, &e, &upE}, {-1, 0, &w, &upW}};
	for (Dir& d : dirs) {
		BlockPos side = pos.offset(d.dx, 0, d.dz);
		if (isRail(level, side) || isRail(level, side.below())) {
			*d.flag = true;
		} else if (isRail(level, side.above())) {
			*d.flag = true;
			*d.up	= true;
		}
	}
	bool		straight = _kind != Kind::Plain;
	std::string next	 = shape(level, state);
	bool		ns = n || s, ew = e || w;
	if (ns && ew && !straight) {
		next = s && e ? "south_east" : s && w ? "south_west" : n && e ? "north_east" : "north_west";
	} else if (ns && (!ew || next.find("north") != std::string::npos || next.find("south") != std::string::npos || straight && !ew)) {
		next = upN ? "ascending_north" : upS ? "ascending_south" : "north_south";
	} else if (ew) {
		next = upE ? "ascending_east" : upW ? "ascending_west" : "east_west";
	}
	int updated = level.gameData().withProperty(state, "shape", next);
	if (updated >= 0 && updated != state) level.setBlock(pos, updated, Level::UPDATE_ALL);
	if (!neighborsToo) return;
	// The rails around reshape to meet it (one level only)
	for (Dir& d : dirs) {
		for (int dy : {0, 1, -1}) {
			BlockPos side = pos.offset(d.dx, dy, d.dz);
			if (!isRail(level, side)) continue;
			const BlockBehavior& behavior = level.behavior(level.getBlockState(side));
			if (auto* rail = dynamic_cast<const RailBehavior*>(&behavior)) rail->connect(level, side, false);
		}
	}
}

bool RailBehavior::findPoweredRailSignal(Level& level, const BlockPos& pos, int block, bool forward, int distance) const {
	// PoweredRailBlock.findPoweredRailSignal: along its line, up to 8 rails of its kind, one powered by redstone
	if (distance >= 8) return false;
	int					state = level.getBlockState(pos);
	std::array<Step, 2> axis  = ends(shape(level, state));
	Step				step  = axis[forward ? 1 : 0];
	for (int dy : {0, 1, -1}) {
		BlockPos next = pos.offset(step.dx, dy, step.dz);
		int		 nextState = level.getBlockState(next);
		if (level.blocks().blockOf(nextState) != block) continue;
		if (level.hasNeighborSignal(next) || level.hasNeighborSignal(next.above())) return true;
		return findPoweredRailSignal(level, next, block, forward, distance + 1);
	}
	return false;
}

void RailBehavior::updatePower(Level& level, const BlockPos& pos) const {
	if (_kind != Kind::Powered && _kind != Kind::Activator) return;
	int	 state	 = level.getBlockState(pos);
	int	 block	 = level.blocks().blockOf(state);
	bool powered = level.hasNeighborSignal(pos) || findPoweredRailSignal(level, pos, block, true, 0) || findPoweredRailSignal(level, pos, block, false, 0);
	int	 next	 = level.gameData().withProperty(state, "powered", powered ? "true" : "false");
	if (next >= 0 && next != state) level.setBlock(pos, next, Level::UPDATE_ALL);
}

void RailBehavior::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (level.blocks().blockOf(oldState) == level.blocks().blockOf(state)) return; // Only a new rail
	connect(level, pos, true);
	updatePower(level, pos);
}

void RailBehavior::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	if (!canSurvive(level, pos, state)) {
		level.destroyBlock(pos, true);
		return;
	}
	updatePower(level, pos);
}

void RailBehavior::minecartOn(Level& level, const BlockPos& pos) {
	int state = level.getBlockState(pos);
	if (blockName(level, state) != "minecraft:detector_rail" || level.gameData().getProperty(state, "powered") == "true") return;
	int on = level.gameData().withProperty(state, "powered", "true");
	if (on < 0) return;
	level.setBlock(pos, on, Level::UPDATE_ALL);
	level.updateNeighborsAt(pos.below(), level.blocks().blockOf(on));
	level.scheduleTick(pos, level.blocks().blockOf(on), 20);
}

void RailBehavior::tick(Level& level, const BlockPos& pos, int state) const {
	if (_kind != Kind::Detector || level.gameData().getProperty(state, "powered") != "true") return;
	// Still a minecart on it: on for another second, else off
	AABB box{pos.x + 0.2, static_cast<double>(pos.y), pos.z + 0.2, pos.x + 0.8, pos.y + 0.8, pos.z + 0.8};
	for (Actor* actor : level.actorsIn(box)) {
		if (dynamic_cast<Minecart*>(actor)) {
			level.scheduleTick(pos, level.blocks().blockOf(state), 20);
			return;
		}
	}
	int off = level.gameData().withProperty(state, "powered", "false");
	if (off < 0) return;
	level.setBlock(pos, off, Level::UPDATE_ALL);
	level.updateNeighborsAt(pos.below(), level.blocks().blockOf(off));
}

int RailBehavior::getSignal(Level& level, const BlockPos&, int state, Direction) const {
	return _kind == Kind::Detector && level.gameData().getProperty(state, "powered") == "true" ? 15 : 0;
}

int RailBehavior::getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const {
	return direction == Direction::Up ? getSignal(level, pos, state, direction) : 0;
}
