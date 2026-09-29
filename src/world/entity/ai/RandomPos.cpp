#include "world/entity/ai/RandomPos.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"

#include <cmath>
#include <limits>

namespace RandomPos {

	BlockPos generateRandomDirection(JavaRandom& random, int horizontal, int vertical) {
		int x = random.nextInt(2 * horizontal + 1) - horizontal;
		int y = random.nextInt(2 * vertical + 1) - vertical;
		int z = random.nextInt(2 * horizontal + 1) - horizontal;
		return {x, y, z};
	}

	std::optional<BlockPos> generateRandomDirectionWithinRadians(JavaRandom& random, int horizontal, int vertical, int yOffset, double dx, double dz,
																 double radians) {
		double angle	= Mth::atan2(dz, dx) - (float)(M_PI / 2);
		double chosen	= angle + (2.0F * random.nextFloat() - 1.0F) * radians;
		double distance = std::sqrt(random.nextDouble()) * static_cast<double>(std::sqrt(2.0F)) * horizontal; // Mth.SQRT_OF_TWO
		double x		= -distance * std::sin(chosen);
		double z		= distance * std::cos(chosen);
		if (std::abs(x) > horizontal || std::abs(z) > horizontal) return std::nullopt;
		int y = random.nextInt(2 * vertical + 1) - vertical + yOffset;
		return BlockPos{Mth::floor(x), y, Mth::floor(z)};
	}

	BlockPos moveUpOutOfSolid(BlockPos pos, int maxY, const std::function<bool(const BlockPos&)>& solid) {
		if (!solid(pos)) return pos;
		BlockPos up = pos.above();
		while (up.y <= maxY && solid(up)) up = up.above();
		return up;
	}

	BlockPos moveUpToAboveSolid(BlockPos pos, int aboveSolid, int maxY, const std::function<bool(const BlockPos&)>& solid) {
		if (!solid(pos)) return pos;
		BlockPos up = pos.above();
		while (up.y <= maxY && solid(up)) up = up.above();
		int start = up.y;
		while (up.y <= maxY && up.y - start < aboveSolid) {
			up = up.above();
			if (solid(up)) {
				up = up.below();
				break;
			}
		}
		return up;
	}

	std::optional<Vec3> generateRandomPos(const Supplier& supplier, const std::function<double(const BlockPos&)>& value) {
		double					best = -std::numeric_limits<double>::infinity();
		std::optional<BlockPos> chosen;
		for (int i = 0; i < 10; i++) {
			std::optional<BlockPos> pos = supplier();
			if (!pos) continue;
			double v = value(*pos);
			if (v > best) {
				best   = v;
				chosen = pos;
			}
		}
		if (!chosen) return std::nullopt;
		return Vec3{chosen->x + 0.5, static_cast<double>(chosen->y), chosen->z + 0.5};
	}

	std::optional<Vec3> generateRandomPos(Mob& mob, const Supplier& supplier) {
		return generateRandomPos(supplier, [&mob](const BlockPos& pos) { return mob.getWalkTargetValue(pos); });
	}

	BlockPos generateRandomPosTowardDirection(Mob& mob, int horizontal, JavaRandom& random, const BlockPos& direction) {
		int x = direction.x, z = direction.z;
		if (mob.hasHome() && horizontal > 1) {
			const BlockPos& home = mob.getHomePosition();
			x += mob.position().x > home.x ? -random.nextInt(horizontal / 2) : random.nextInt(horizontal / 2);
			z += mob.position().z > home.z ? -random.nextInt(horizontal / 2) : random.nextInt(horizontal / 2);
		}
		return {Mth::floor(x + mob.position().x), Mth::floor(direction.y + mob.position().y), Mth::floor(z + mob.position().z)};
	}

	// ----- GoalUtils -----

	bool mobRestricted(Mob& mob, int radius) {
		if (!mob.hasHome()) return false;
		const BlockPos& home   = mob.getHomePosition();
		double			reach  = mob.getHomeRadius() + radius + 1;
		Vec3			center{home.x + 0.5, home.y + 0.5, home.z + 0.5};
		return (center - mob.position()).lengthSqr() < reach * reach; // closerToCenterThan
	}
	bool isOutsideLimits(const BlockPos& pos, Mob& mob) { return mob.level().isOutsideBuildHeight(pos.y); }
	bool isRestricted(bool restricted, Mob& mob, const BlockPos& pos) { return restricted && !mob.isWithinHome(pos); }
	bool isNotStable(Mob& mob, const BlockPos& pos) { return !mob.navigation().isStableDestination(pos); }
	bool isWater(Mob& mob, const BlockPos& pos) { return mob.level().fluids().isWater(mob.level().getFluidState(pos).type); }
	bool hasMalus(Mob& mob, const BlockPos& pos) { return mob.getPathfindingMalus(WalkNodeEvaluator::getPathTypeStatic(mob.level(), mob, pos)) != 0.0F; }
	bool isSolid(Mob& mob, const BlockPos& pos) { return mob.level().gameData().getStateProperties(mob.level().getBlockState(pos)).solid; }

	// ----- DefaultRandomPos -----

	namespace {
		std::optional<BlockPos> defaultToward(Mob& mob, int horizontal, bool restricted, const BlockPos& direction) {
			BlockPos pos = generateRandomPosTowardDirection(mob, horizontal, mob.random(), direction);
			if (isOutsideLimits(pos, mob) || isRestricted(restricted, mob, pos) || isNotStable(mob, pos) || hasMalus(mob, pos)) return std::nullopt;
			return pos;
		}
	} // namespace

	std::optional<Vec3> defaultPos(Mob& mob, int horizontal, int vertical) {
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(mob, [&]() { return defaultToward(mob, horizontal, restricted, generateRandomDirection(mob.random(), horizontal, vertical)); });
	}

	std::optional<Vec3> defaultPosTowards(Mob& mob, int horizontal, int vertical, const Vec3& target, double radians) {
		Vec3 direction	= target - mob.position();
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(mob, [&]() -> std::optional<BlockPos> {
			std::optional<BlockPos> d = generateRandomDirectionWithinRadians(mob.random(), horizontal, vertical, 0, direction.x, direction.z, radians);
			return d ? defaultToward(mob, horizontal, restricted, *d) : std::nullopt;
		});
	}

	std::optional<Vec3> defaultPosAway(Mob& mob, int horizontal, int vertical, const Vec3& from) {
		Vec3 direction	= mob.position() - from;
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(mob, [&]() -> std::optional<BlockPos> {
			std::optional<BlockPos> d = generateRandomDirectionWithinRadians(mob.random(), horizontal, vertical, 0, direction.x, direction.z, (float)(M_PI / 2));
			return d ? defaultToward(mob, horizontal, restricted, *d) : std::nullopt;
		});
	}

	// ----- LandRandomPos -----

	std::optional<BlockPos> landGenerateRandomPosTowardDirection(Mob& mob, int horizontal, bool restricted, const BlockPos& direction) {
		BlockPos pos = generateRandomPosTowardDirection(mob, horizontal, mob.random(), direction);
		if (isOutsideLimits(pos, mob) || isRestricted(restricted, mob, pos) || isNotStable(mob, pos)) return std::nullopt;
		return pos;
	}

	std::optional<BlockPos> movePosUpOutOfSolid(Mob& mob, const BlockPos& start) {
		BlockPos pos = moveUpOutOfSolid(start, mob.level().maxY() - 1, [&](const BlockPos& at) { return isSolid(mob, at); });
		if (isWater(mob, pos) || hasMalus(mob, pos)) return std::nullopt;
		return pos;
	}

	std::optional<Vec3> landPos(Mob& mob, int horizontal, int vertical, const std::function<double(const BlockPos&)>& value) {
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(
				[&]() -> std::optional<BlockPos> {
					BlockPos				direction = generateRandomDirection(mob.random(), horizontal, vertical);
					std::optional<BlockPos> pos		  = landGenerateRandomPosTowardDirection(mob, horizontal, restricted, direction);
					return pos ? movePosUpOutOfSolid(mob, *pos) : std::nullopt;
				},
				value);
	}

	std::optional<Vec3> landPos(Mob& mob, int horizontal, int vertical) {
		return landPos(mob, horizontal, vertical, [&mob](const BlockPos& pos) { return mob.getWalkTargetValue(pos); });
	}

	namespace {
		std::optional<Vec3> landPosInDirection(Mob& mob, int horizontal, int vertical, const Vec3& direction, bool restricted) {
			return generateRandomPos(mob, [&]() -> std::optional<BlockPos> {
				std::optional<BlockPos> d = generateRandomDirectionWithinRadians(mob.random(), horizontal, vertical, 0, direction.x, direction.z, (float)(M_PI / 2));
				if (!d) return std::nullopt;
				std::optional<BlockPos> pos = landGenerateRandomPosTowardDirection(mob, horizontal, restricted, *d);
				return pos ? movePosUpOutOfSolid(mob, *pos) : std::nullopt;
			});
		}
	} // namespace

	std::optional<Vec3> landPosTowards(Mob& mob, int horizontal, int vertical, const Vec3& target) {
		return landPosInDirection(mob, horizontal, vertical, target - mob.position(), mobRestricted(mob, horizontal));
	}

	std::optional<Vec3> landPosAway(Mob& mob, int horizontal, int vertical, const Vec3& from) {
		return landPosInDirection(mob, horizontal, vertical, mob.position() - from, mobRestricted(mob, horizontal));
	}

	// ----- Air and water -----

	std::optional<BlockPos> airAndWaterGenerate(Mob& mob, int horizontal, int vertical, int yOffset, double dx, double dz, double radians, bool restricted) {
		std::optional<BlockPos> d = generateRandomDirectionWithinRadians(mob.random(), horizontal, vertical, yOffset, dx, dz, radians);
		if (!d) return std::nullopt;
		BlockPos pos = generateRandomPosTowardDirection(mob, horizontal, mob.random(), *d);
		if (isOutsideLimits(pos, mob) || isRestricted(restricted, mob, pos)) return std::nullopt;
		pos = moveUpOutOfSolid(pos, mob.level().maxY() - 1, [&](const BlockPos& at) { return isSolid(mob, at); });
		if (hasMalus(mob, pos)) return std::nullopt;
		return pos;
	}

	std::optional<Vec3> airAndWaterPos(Mob& mob, int horizontal, int vertical, int yOffset, double dx, double dz, double radians) {
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(mob, [&]() { return airAndWaterGenerate(mob, horizontal, vertical, yOffset, dx, dz, radians, restricted); });
	}

	std::optional<Vec3> airPosTowards(Mob& mob, int horizontal, int vertical, int yOffset, const Vec3& target, double radians) {
		Vec3 direction	= target - mob.position();
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(mob, [&]() -> std::optional<BlockPos> {
			std::optional<BlockPos> pos = airAndWaterGenerate(mob, horizontal, vertical, yOffset, direction.x, direction.z, radians, restricted);
			return pos && !isWater(mob, *pos) ? pos : std::nullopt;
		});
	}

	std::optional<Vec3> hoverPos(Mob& mob, int horizontal, int vertical, double dx, double dz, float radians, int maxAbove, int minAbove) {
		bool restricted = mobRestricted(mob, horizontal);
		return generateRandomPos(mob, [&]() -> std::optional<BlockPos> {
			std::optional<BlockPos> d = generateRandomDirectionWithinRadians(mob.random(), horizontal, vertical, 0, dx, dz, radians);
			if (!d) return std::nullopt;
			std::optional<BlockPos> pos = landGenerateRandomPosTowardDirection(mob, horizontal, restricted, *d);
			if (!pos) return std::nullopt;
			BlockPos above = moveUpToAboveSolid(*pos, mob.random().nextInt(maxAbove - minAbove + 1) + minAbove, mob.level().maxY() - 1,
												[&](const BlockPos& at) { return isSolid(mob, at); });
			if (isWater(mob, above) || hasMalus(mob, above)) return std::nullopt;
			return above;
		});
	}

} // namespace RandomPos
