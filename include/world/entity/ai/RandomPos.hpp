#ifndef RANDOM_POS_HPP
#define RANDOM_POS_HPP

#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

#include <functional>
#include <optional>

class JavaRandom;
class Mob;

// Random destinations for wandering, fleeing and flying goals (vanilla's net.minecraft.world.entity.ai.util:
// RandomPos, DefaultRandomPos, LandRandomPos, AirRandomPos, HoverRandomPos, AirAndWaterRandomPos, GoalUtils)
namespace RandomPos {
	using Supplier = std::function<std::optional<BlockPos>()>;

	BlockPos				generateRandomDirection(JavaRandom& random, int horizontal, int vertical);
	std::optional<BlockPos> generateRandomDirectionWithinRadians(JavaRandom& random, int horizontal, int vertical, int yOffset, double dx, double dz,
																 double radians);
	BlockPos				moveUpOutOfSolid(BlockPos pos, int maxY, const std::function<bool(const BlockPos&)>& solid);
	BlockPos				moveUpToAboveSolid(BlockPos pos, int aboveSolid, int maxY, const std::function<bool(const BlockPos&)>& solid);
	// The best of 10 tries, by the mob's walk target value (or the given one)
	std::optional<Vec3>		generateRandomPos(Mob& mob, const Supplier& supplier);
	std::optional<Vec3>		generateRandomPos(const Supplier& supplier, const std::function<double(const BlockPos&)>& value);
	BlockPos				generateRandomPosTowardDirection(Mob& mob, int horizontal, JavaRandom& random, const BlockPos& direction);

	// DefaultRandomPos
	std::optional<Vec3> defaultPos(Mob& mob, int horizontal, int vertical);
	std::optional<Vec3> defaultPosTowards(Mob& mob, int horizontal, int vertical, const Vec3& target, double radians);
	std::optional<Vec3> defaultPosAway(Mob& mob, int horizontal, int vertical, const Vec3& from);
	// LandRandomPos
	std::optional<Vec3>		landPos(Mob& mob, int horizontal, int vertical);
	std::optional<Vec3>		landPos(Mob& mob, int horizontal, int vertical, const std::function<double(const BlockPos&)>& value);
	std::optional<Vec3>		landPosTowards(Mob& mob, int horizontal, int vertical, const Vec3& target);
	std::optional<Vec3>		landPosAway(Mob& mob, int horizontal, int vertical, const Vec3& from);
	std::optional<BlockPos> landGenerateRandomPosTowardDirection(Mob& mob, int horizontal, bool restricted, const BlockPos& direction);
	std::optional<BlockPos> movePosUpOutOfSolid(Mob& mob, const BlockPos& pos);
	// AirRandomPos, HoverRandomPos, AirAndWaterRandomPos
	std::optional<Vec3> airPosTowards(Mob& mob, int horizontal, int vertical, int yOffset, const Vec3& target, double radians);
	std::optional<Vec3> hoverPos(Mob& mob, int horizontal, int vertical, double dx, double dz, float radians, int maxAbove, int minAbove);
	std::optional<Vec3> airAndWaterPos(Mob& mob, int horizontal, int vertical, int yOffset, double dx, double dz, double radians);
	std::optional<BlockPos> airAndWaterGenerate(Mob& mob, int horizontal, int vertical, int yOffset, double dx, double dz, double radians, bool restricted);

	// GoalUtils
	bool mobRestricted(Mob& mob, int radius);
	bool isOutsideLimits(const BlockPos& pos, Mob& mob);
	bool isRestricted(bool restricted, Mob& mob, const BlockPos& pos);
	bool isNotStable(Mob& mob, const BlockPos& pos);
	bool isWater(Mob& mob, const BlockPos& pos);
	bool hasMalus(Mob& mob, const BlockPos& pos);
	bool isSolid(Mob& mob, const BlockPos& pos);
} // namespace RandomPos

#endif
