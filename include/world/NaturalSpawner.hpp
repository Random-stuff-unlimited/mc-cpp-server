#ifndef NATURAL_SPAWNER_HPP
#define NATURAL_SPAWNER_HPP

class Level;

// Natural mob spawning (vanilla's NaturalSpawner, with ServerChunkCache.tickChunks' part of it): every tick, in each
// chunk within 128 blocks of a player (shuffled), each mob category under its caps — global (per 17x17 chunks around
// the players) and local (per player) — tries to spawn a pack of a mob picked from the biome's spawners, at a random
// position of the chunk, 24 to 128 blocks away from the players. Creatures (animals) only try every 400 ticks
// (persistent categories); spawn costs (soul sand valleys...) limit how dense a type gets
namespace NaturalSpawner {
	// One tick of it. spawnEnemies: monsters too (not in peaceful)
	void tick(Level& level, bool spawnFriendlies, bool spawnEnemies);
} // namespace NaturalSpawner

#endif
