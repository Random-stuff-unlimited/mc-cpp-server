#include "world/entity/mobs/Monster.hpp"

#include "world/Level.hpp"

void Monster::aiStep() {
	// updateSwingTime: the swing animation is the client's
	updateNoActionTime();
	Mob::aiStep();
}

void Monster::updateNoActionTime() {
	if (lightLevelDependentMagicValue() > 0.5F) _noActionTime += 2;
}

float Monster::getWalkTargetValue(const BlockPos& pos) { return -_level.getPathfindingCostFromLightLevels(pos); }
