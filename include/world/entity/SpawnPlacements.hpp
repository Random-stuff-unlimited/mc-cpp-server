#ifndef SPAWN_PLACEMENTS_HPP
#define SPAWN_PLACEMENTS_HPP

#include "world/BlockPos.hpp"
#include "world/Level.hpp"
#include "world/entity/MobRegistry.hpp"

class JavaRandom;
struct AABB;

// Where each mob type may spawn (vanilla's SpawnPlacements, SpawnPlacementTypes and the check*SpawnRules of the mob
// classes): its placement (on the ground, in water, in lava, anywhere), the heightmap natural spawning starts from, and
// its own rules (darkness for monsters, grass for animals, water depth for drowned...)
namespace SpawnPlacements {
	using SpawnReason = MobRegistry::SpawnReason;
	enum class Placement { NoRestrictions, InWater, InLava, OnGround };

	Placement		  placementType(Level& level, int typeId);
	Level::Heightmap  heightmapType(Level& level, int typeId);
	// SpawnPlacements.isSpawnPositionOk: the placement type allows this position
	bool			  isSpawnPositionOk(Level& level, int typeId, const BlockPos& pos);
	// SpawnPlacementType.adjustSpawnPosition: on the ground, one lower if that block can be walked through
	BlockPos		  adjustSpawnPosition(Level& level, Placement placement, const BlockPos& pos);
	// SpawnPlacements.checkSpawnRules: the type's own rules (true for types without)
	bool			  checkSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos, JavaRandom& random);

	// BlockState.isValidSpawn for a type (the block below a spawn)
	bool isValidSpawn(Level& level, int state, int typeId);
	// NaturalSpawner.isValidEmptySpawnBlock: a block a mob can stand in
	bool isValidEmptySpawnBlock(Level& level, const BlockPos& pos, int state, int typeId);
	// EntityType.isBlockDangerous, with the type's immunities (foxes and berry bushes, strays and powder snow...)
	bool isBlockDangerous(Level& level, int state, int typeId);
	// Monster.isDarkEnoughToSpawn
	bool isDarkEnoughToSpawn(Level& level, const BlockPos& pos, JavaRandom& random);
	// Mob.checkMobSpawnRules, Monster.checkMonsterSpawnRules, Monster.checkAnyLightMonsterSpawnRules,
	// Animal.checkAnimalSpawnRules
	bool checkMobSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos);
	bool checkMonsterSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos, JavaRandom& random);
	bool checkAnyLightMonsterSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos);
	bool checkAnimalSpawnRules(Level& level, SpawnReason reason, const BlockPos& pos);
	// Animal.isBrightEnoughToSpawn
	bool isBrightEnoughToSpawn(Level& level, const BlockPos& pos);
	// BlockGetter.containsAnyLiquid
	bool containsAnyLiquid(Level& level, const AABB& box);
	// WorldgenRandom.seedSlimeChunk(...).nextInt(10) == 0
	bool isSlimeChunk(int64_t seed, int chunkX, int chunkZ);
} // namespace SpawnPlacements

#endif
