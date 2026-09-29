#ifndef DISMOUNT_HELPER_HPP
#define DISMOUNT_HELPER_HPP

#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

#include <optional>

class Level;

// Where an entity can stand after leaving a bed, a respawn anchor or a vehicle (vanilla's DismountHelper), and the
// blocks it must not stand in (EntityType.isBlockDangerous)
namespace DismountHelper {
	// DismountHelper.offsetsForDirection: the cells around a vehicle facing `direction`, as (x, z) steps
	std::array<std::array<int, 2>, 8> offsetsForDirection(Direction direction);
	// DismountHelper.findSafeDismountLocation: the feet position at the bottom center of `pos` (raised onto its floor)
	// for an entity of this size, if it fits there. avoidDanger: not onto fire, lava, cactus... (isBlockDangerous)
	std::optional<Vec3> findSafeDismountLocation(Level& level, float width, float height, bool fireImmune, bool isPlayer, const BlockPos& pos,
												 bool avoidDanger);
	// DismountHelper.canDismountTo: no block collides with the box
	bool canDismountTo(Level& level, const AABB& box);
	// EntityType.isBlockDangerous
	bool isBlockDangerous(Level& level, int state, bool fireImmune);
	// NodeEvaluator.isBurningBlock: fire, lava, magma, a lit campfire, a lava cauldron
	bool isBurningBlock(Level& level, int state);
	// BlockGetter.getBlockFloorHeight(pos): the top of the block's collision (without climbable blocks and open
	// trapdoors), or of the one below minus 1, -infinity if nothing to stand on
	double blockFloorHeight(Level& level, const BlockPos& pos);
} // namespace DismountHelper

#endif
