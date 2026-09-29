#include "world/entity/DismountHelper.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"

#include <cmath>
#include <limits>

namespace {
	// DismountHelper.nonClimbableShape: the collision boxes of the block, none for climbable blocks and open trapdoors
	const std::vector<GameData::Box>& nonClimbableShape(Level& level, const BlockPos& pos) {
		static const std::vector<GameData::Box> EMPTY;
		const GameData& data  = level.gameData();
		int				state = level.getBlockState(pos);
		int				block = level.blocks().blockOf(state);
		if (data.isInTag("minecraft:block", "minecraft:climbable", block)) return EMPTY;
		if (data.isInstanceOf(block, "TrapDoorBlock") && level.blocks().getBool(state, level.blocks().property("open"))) return EMPTY;
		return data.getCollisionShape(state);
	}

	double maxY(const std::vector<GameData::Box>& boxes) {
		double max = -std::numeric_limits<double>::infinity();
		for (const GameData::Box& box : boxes) max = std::max(max, box.maxY);
		return max;
	}

	int stepX(Direction direction) { return Directions::stepX(direction); }
	int stepZ(Direction direction) { return Directions::stepZ(direction); }
} // namespace

namespace DismountHelper {

	std::array<std::array<int, 2>, 8> offsetsForDirection(Direction direction) {
		Direction right	   = Directions::clockWise(direction);
		Direction left	   = Directions::opposite(right);
		Direction back	   = Directions::opposite(direction);
		return {{{stepX(right), stepZ(right)},
				 {stepX(left), stepZ(left)},
				 {stepX(back) + stepX(right), stepZ(back) + stepZ(right)},
				 {stepX(back) + stepX(left), stepZ(back) + stepZ(left)},
				 {stepX(direction) + stepX(right), stepZ(direction) + stepZ(right)},
				 {stepX(direction) + stepX(left), stepZ(direction) + stepZ(left)},
				 {stepX(back), stepZ(back)},
				 {stepX(direction), stepZ(direction)}}};
	}

	bool isBurningBlock(Level& level, int state) {
		const GameData& data  = level.gameData();
		int				block = level.blocks().blockOf(state);
		static thread_local const GameData* cached = nullptr;
		static thread_local int				lava = -1, magma = -1, lavaCauldron = -1;
		if (cached != &data) {
			cached		 = &data;
			lava		 = data.getStaticId("minecraft:block", "minecraft:lava");
			magma		 = data.getStaticId("minecraft:block", "minecraft:magma_block");
			lavaCauldron = data.getStaticId("minecraft:block", "minecraft:lava_cauldron");
		}
		if (block == lava || block == magma || block == lavaCauldron || data.isInTag("minecraft:block", "minecraft:fire", block)) return true;
		// CampfireBlock.isLitCampfire
		return data.isInTag("minecraft:block", "minecraft:campfires", block) && level.blocks().getBool(state, level.blocks().property("lit"));
	}

	bool isBlockDangerous(Level& level, int state, bool fireImmune) {
		if (!fireImmune && isBurningBlock(level, state)) return true;
		const std::string& name = level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state));
		return name == "minecraft:wither_rose" || name == "minecraft:sweet_berry_bush" || name == "minecraft:cactus" || name == "minecraft:powder_snow";
	}

	double blockFloorHeight(Level& level, const BlockPos& pos) {
		const std::vector<GameData::Box>& shape = nonClimbableShape(level, pos);
		if (!shape.empty()) return maxY(shape);
		double below = maxY(nonClimbableShape(level, pos.below()));
		return below >= 1.0 ? below - 1.0 : -std::numeric_limits<double>::infinity();
	}

	bool canDismountTo(Level& level, const AABB& box) { return !level.hasBlockCollision(box); }

	std::optional<Vec3> findSafeDismountLocation(Level& level, float width, float height, bool fireImmune, bool isPlayer, const BlockPos& pos,
												 bool avoidDanger) {
		if (avoidDanger && isBlockDangerous(level, level.getBlockState(pos), fireImmune)) return std::nullopt;
		double floor = blockFloorHeight(level, pos);
		// isBlockFloorValid
		if (std::isinf(floor) || floor >= 1.0) return std::nullopt;
		if (avoidDanger && floor <= 0.0 && isBlockDangerous(level, level.getBlockState(pos.below()), fireImmune)) return std::nullopt;
		Vec3 feet{pos.x + 0.5, pos.y + floor, pos.z + 0.5}; // Vec3.upFromBottomCenterOf
		// EntityDimensions.makeBoundingBox (floats)
		double half = width / 2.0F;
		AABB   box{feet.x - half, feet.y, feet.z - half, feet.x + half, feet.y + height, feet.z + half};
		if (level.hasBlockCollision(box)) return std::nullopt;
		if (isPlayer) {
			const GameData& data = level.gameData();
			if (data.isInTag("minecraft:block", "minecraft:invalid_spawn_inside", level.blocks().blockOf(level.getBlockState(pos))) ||
				data.isInTag("minecraft:block", "minecraft:invalid_spawn_inside", level.blocks().blockOf(level.getBlockState(pos.above())))) {
				return std::nullopt;
			}
		}
		return feet;
	}

} // namespace DismountHelper
