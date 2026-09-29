#include "world/NaturalSpawner.hpp"

#include "data/GameData.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Chunk.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/SpawnPlacements.hpp"

#include <algorithm>
#include <cstdio>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>

namespace NaturalSpawner {

	namespace {
		using Category = GameData::MobCategory;
		constexpr int CATEGORY_COUNT = 8;
		constexpr int MAGIC_NUMBER	 = 17 * 17;

		// MobCategory: max instances per chunk, friendly, persistent, despawn distance
		struct CategoryInfo {
			int	 max;
			bool friendly, persistent;
			int	 despawnDistance;
		};
		constexpr std::array<CategoryInfo, CATEGORY_COUNT> CATEGORIES = {{
			{70, false, false, 128}, // Monster
			{10, true, true, 128},	 // Creature
			{15, true, false, 128},	 // Ambient
			{5, true, false, 128},	 // Axolotls
			{5, true, false, 128},	 // UndergroundWaterCreature
			{5, true, false, 128},	 // WaterCreature
			{20, true, false, 64},	 // WaterAmbient
			{-1, true, true, 128},	 // Misc
		}};

		int64_t chunkKey(int x, int z) { return (static_cast<int64_t>(z) << 32) | static_cast<uint32_t>(x); }

		bool isSpawningPlayer(const Player& player) { return !player.isDisconnected() && !player.isSpectator(); }

		// ChunkMap.playerIsCloseEnoughForSpawning: within 128 blocks of the chunk's center, horizontally
		bool closeForSpawning(const Player& player, int chunkX, int chunkZ) {
			double dx = player.getX() - (chunkX * 16 + 8), dz = player.getZ() - (chunkZ * 16 + 8);
			return dx * dx + dz * dz < 128.0 * 128.0;
		}

		// PotentialCalculator
		struct PotentialCalculator {
			std::vector<std::pair<BlockPos, double>> charges;
			void addCharge(const BlockPos& pos, double charge) {
				if (charge != 0.0) charges.emplace_back(pos, charge);
			}
			double getPotentialEnergyChange(const BlockPos& pos, double charge) const {
				if (charge == 0.0) return 0.0;
				double sum = 0.0;
				for (const auto& [at, c] : charges) {
					double dx = at.x - pos.x, dy = at.y - pos.y, dz = at.z - pos.z;
					double d  = dx * dx + dy * dy + dz * dz;
					sum += d == 0.0 ? std::numeric_limits<double>::infinity() : c / std::sqrt(d);
				}
				return sum * charge;
			}
		};

		// LocalMobCapCalculator: per player, the mobs in the chunks it is close enough to spawn in
		struct LocalMobCapCalculator {
			Level&													   level;
			std::unordered_map<int64_t, std::vector<const Player*>>	   playersNear;
			std::map<const Player*, std::array<int, CATEGORY_COUNT>> counts;

			const std::vector<const Player*>& getPlayersNear(int chunkX, int chunkZ) {
				auto [it, inserted] = playersNear.try_emplace(chunkKey(chunkX, chunkZ));
				if (inserted) {
					for (const auto& player : level.players()) {
						if (isSpawningPlayer(*player) && closeForSpawning(*player, chunkX, chunkZ)) it->second.push_back(player.get());
					}
				}
				return it->second;
			}
			void addMob(int chunkX, int chunkZ, int category) {
				for (const Player* player : getPlayersNear(chunkX, chunkZ)) {
					auto [it, inserted] = counts.try_emplace(player);
					if (inserted) it->second.fill(0);
					it->second[category]++;
				}
			}
			bool canSpawn(int category, int chunkX, int chunkZ) {
				for (const Player* player : getPlayersNear(chunkX, chunkZ)) {
					auto it = counts.find(player);
					if (it == counts.end() || it->second[category] < CATEGORIES[category].max) return true;
				}
				return false;
			}
		};

		// NaturalSpawner.SpawnState
		struct SpawnState {
			int									 spawnableChunkCount;
			std::array<int, CATEGORY_COUNT>		 mobCategoryCounts{};
			PotentialCalculator					 spawnPotential;
			LocalMobCapCalculator				 localMobCaps;
			std::optional<BlockPos>				 lastCheckedPos;
			int									 lastCheckedType = -1;
			double								 lastCharge		 = 0.0;

			bool canSpawnForCategoryGlobal(int category) const {
				int cap = CATEGORIES[category].max * spawnableChunkCount / MAGIC_NUMBER;
				return mobCategoryCounts[category] < cap;
			}
		};

		// The spawn cost of a type in the biome at pos (NaturalSpawner.getRoughBiome), null if none
		const std::pair<double, double>* spawnCost(Level& level, const BlockPos& pos, int typeId) {
			const GameData::Biome& biome = level.getBiome(pos);
			auto				   it	 = biome.spawnCosts.find(typeId);
			return it == biome.spawnCosts.end() ? nullptr : &it->second;
		}

		bool canSpawn(Level& level, SpawnState& state, int typeId, const BlockPos& pos) {
			state.lastCheckedPos  = pos;
			state.lastCheckedType = typeId;
			const auto* cost	  = spawnCost(level, pos, typeId);
			if (!cost) {
				state.lastCharge = 0.0;
				return true;
			}
			state.lastCharge = cost->second;
			return state.spawnPotential.getPotentialEnergyChange(pos, cost->second) <= cost->first;
		}

		void afterSpawn(Level& level, SpawnState& state, Mob& mob) {
			BlockPos pos = mob.blockPosition();
			double	 charge;
			if (state.lastCheckedPos && pos == *state.lastCheckedPos && mob.typeId() == state.lastCheckedType) {
				charge = state.lastCharge;
			} else {
				const auto* cost = spawnCost(level, pos, mob.typeId());
				charge			 = cost ? cost->second : 0.0;
			}
			state.spawnPotential.addCharge(pos, charge);
			int category = static_cast<int>(mob.type().category);
			state.mobCategoryCounts[category]++;
			state.localMobCaps.addMob(pos.chunkX(), pos.chunkZ(), category);
		}

		// DistanceManager.getNaturalSpawnChunkCount: the chunks within 8 chunks of a player (not a spectator)
		int naturalSpawnChunkCount(Level& level) {
			std::unordered_map<int64_t, bool> chunks;
			for (const auto& player : level.players()) {
				if (!isSpawningPlayer(*player)) continue;
				int cx = Mth::floor(player->getX()) >> 4, cz = Mth::floor(player->getZ()) >> 4;
				for (int x = cx - 8; x <= cx + 8; x++) {
					for (int z = cz - 8; z <= cz + 8; z++) chunks[chunkKey(x, z)] = true;
				}
			}
			return static_cast<int>(chunks.size());
		}

		// NaturalSpawner.createState
		SpawnState createState(Level& level) {
			SpawnState state{naturalSpawnChunkCount(level), {}, {}, {level, {}, {}}, std::nullopt, -1, 0.0};
			level.entities().forEach([&](Entity& entity) {
				auto* mob = dynamic_cast<Mob*>(&entity);
				if (!mob || mob->isRemoved()) {
					if (!mob) return; // Only mobs have a category other than misc
					return;
				}
				if (mob->isPersistenceRequired()) return;
				int category = static_cast<int>(mob->type().category);
				if (category == static_cast<int>(Category::Misc)) return;
				BlockPos pos = mob->blockPosition();
				if (!level.loadedChunk(pos.chunkX(), pos.chunkZ())) return;
				if (const auto* cost = spawnCost(level, pos, mob->typeId())) state.spawnPotential.addCharge(pos, cost->second);
				state.localMobCaps.addMob(pos.chunkX(), pos.chunkZ(), category);
				state.mobCategoryCounts[category]++;
			});
			return state;
		}

		// WeightedList.getRandom
		const GameData::SpawnerData* getRandom(const std::vector<GameData::SpawnerData>& list, JavaRandom& random) {
			int total = 0;
			for (const auto& entry : list) total += entry.weight;
			if (total <= 0) return nullptr;
			int r = random.nextInt(total);
			for (const auto& entry : list) {
				r -= entry.weight;
				if (r < 0) return &entry;
			}
			return nullptr;
		}

		// The spawners of the biome at pos for a category (ChunkGenerator.getMobsAt; structures, such as fortresses,
		// aren't generated here)
		const std::vector<GameData::SpawnerData>& mobsAt(Level& level, int category, const BlockPos& pos) {
			return level.getBiome(pos).spawners[category];
		}

		bool isRightDistanceToPlayerAndSpawnPoint(Level& level, int chunkX, int chunkZ, const BlockPos& pos, double distanceSqr) {
			if (distanceSqr <= 576.0) return false;
			// The world spawn: 24 blocks around it stay free, in the overworld
			if (&level.world() == &level.server().getWorld()) {
				const World::Spawn& spawn = level.world().getSpawn();
				BlockPos			sp{Mth::floor(spawn.x), Mth::floor(spawn.y), Mth::floor(spawn.z)};
				double				dx = sp.x + 0.5 - (pos.x + 0.5), dy = sp.y + 0.5 - pos.y, dz = sp.z + 0.5 - (pos.z + 0.5);
				if (dx * dx + dy * dy + dz * dz < 24.0 * 24.0) return false;
			}
			if (pos.chunkX() == chunkX && pos.chunkZ() == chunkZ) return true;
			Chunk* chunk = level.loadedChunk(pos.chunkX(), pos.chunkZ());
			return chunk && chunk->isTicking(); // canSpawnEntitiesInChunk
		}

		bool isValidSpawnPositionForType(Level& level, int category, const GameData::SpawnerData& spawner, const BlockPos& pos, double distanceSqr) {
			const GameData::EntityTypeInfo* type = level.gameData().getEntityType(spawner.typeId);
			if (!type || type->category == Category::Misc) return false;
			int despawn = CATEGORIES[static_cast<int>(type->category)].despawnDistance;
			// canSpawnFarFromPlayer: the misc and persistent categories, and wandering traders
			bool farOk = CATEGORIES[static_cast<int>(type->category)].persistent ||
						 level.gameData().getStaticName("minecraft:entity_type", spawner.typeId) == "minecraft:wandering_trader";
			if (!farOk && distanceSqr > static_cast<double>(despawn) * despawn) return false;
			if (!type->summonable) return false;
			// canSpawnMobAt: still one of the spawners at that position
			const auto& list = mobsAt(level, category, pos);
			if (std::none_of(list.begin(), list.end(), [&](const GameData::SpawnerData& s) {
					return s.typeId == spawner.typeId && s.weight == spawner.weight && s.minCount == spawner.minCount && s.maxCount == spawner.maxCount;
				})) {
				return false;
			}
			if (!SpawnPlacements::isSpawnPositionOk(level, spawner.typeId, pos)) return false;
			if (!SpawnPlacements::checkSpawnRules(level, spawner.typeId, MobRegistry::SpawnReason::Natural, pos, level.random())) return false;
			// EntityType.getSpawnAABB
			double half = type->width / 2.0, x = pos.x + 0.5, z = pos.z + 0.5;
			return !level.hasBlockCollision({x - half, static_cast<double>(pos.y), z - half, x + half, pos.y + type->height, z + half});
		}

		// NaturalSpawner.spawnCategoryForPosition
		void spawnCategoryForPosition(Level& level, int category, int chunkX, int chunkZ, const BlockPos& start, SpawnState& state) {
			JavaRandom& random = level.random();
			int			y	   = start.y;
			if (level.isRedstoneConductor(level.getBlockState(start))) return;
			int total = 0;
			for (int pack = 0; pack < 3; pack++) {
				int							  x = start.x, z = start.z;
				const GameData::SpawnerData*  spawner = nullptr;
				std::shared_ptr<SpawnGroupData> group;
				int							  count	   = Mth::ceil(random.nextFloat() * 4.0F);
				int							  inGroup  = 0;
				for (int i = 0; i < count; i++) {
					x += random.nextInt(6) - random.nextInt(6);
					z += random.nextInt(6) - random.nextInt(6);
					BlockPos pos{x, y, z};
					double	 px = x + 0.5, pz = z + 0.5;
					// Level.getNearestPlayer(x, y, z, -1, false): any player that isn't a spectator
					double nearest = -1.0;
					for (const auto& player : level.players()) {
						if (player->isDisconnected() || player->isSpectator()) continue;
						double dx = player->getX() - px, dy = player->getY() - y, dz = player->getZ() - pz;
						double d  = dx * dx + dy * dy + dz * dz;
						if (nearest < 0.0 || d < nearest) nearest = d;
					}
					if (nearest < 0.0) continue;
					if (!isRightDistanceToPlayerAndSpawnPoint(level, chunkX, chunkZ, pos, nearest)) continue;
					if (!spawner) {
						// getRandomSpawnMobAt: water ambient mobs are rare in the biomes that say so
						if (category == static_cast<int>(Category::WaterAmbient) &&
							level.gameData().isInTag("minecraft:worldgen/biome", "minecraft:reduced_water_ambient_spawns", level.getBiomeId(pos)) &&
							random.nextFloat() < 0.98F) {
							break;
						}
						spawner = getRandom(mobsAt(level, category, pos), random);
						if (!spawner) break;
						count = spawner->minCount + random.nextInt(1 + spawner->maxCount - spawner->minCount);
					}
					if (!isValidSpawnPositionForType(level, category, *spawner, pos, nearest) || !canSpawn(level, state, spawner->typeId, pos)) continue;
					std::unique_ptr<Mob> mob = level.mobs().create(level, spawner->typeId);
					if (!mob) return;
					mob->snapTo({px, static_cast<double>(y), pz}, random.nextFloat() * 360.0F, 0.0F);
					// isValidPositionForMob
					int	 despawn  = CATEGORIES[static_cast<int>(mob->type().category)].despawnDistance;
					bool tooFar	  = nearest > static_cast<double>(despawn) * despawn && mob->removeWhenFarAway(nearest);
					int	 reason	  = static_cast<int>(MobRegistry::SpawnReason::Natural);
					if (tooFar || !mob->checkSpawnRules(reason) || !mob->checkSpawnObstruction()) continue;
					DifficultyInstance difficulty = level.getCurrentDifficultyAt(mob->blockPosition());
					group						  = mob->finalizeSpawn(difficulty, reason, group);
					total++;
					inGroup++;
					int	 maxCluster = mob->getMaxSpawnClusterSize();
					bool groupFull	= mob->isMaxGroupSizeReached(inGroup);
					Mob* added		= static_cast<Mob*>(level.addFreshEntity(std::move(mob)));
					afterSpawn(level, state, *added);
					if (total >= maxCluster) return;
					if (groupFull) break;
				}
			}
		}
	} // namespace

	void tick(Level& level, bool spawnFriendlies, bool spawnEnemies) {
		SpawnState state = createState(level);
		// getFilteredSpawningCategories: persistent ones (creatures) every 400 ticks
		bool			 spawnPersistent = level.getGameTime() % 400 == 0;
		std::vector<int> categories;
		for (int c = 0; c < CATEGORY_COUNT; c++) {
			if (c == static_cast<int>(Category::Misc)) continue;
			const CategoryInfo& info = CATEGORIES[c];
			if ((spawnFriendlies || !info.friendly) && (spawnEnemies || info.friendly) && (spawnPersistent || !info.persistent) &&
				state.canSpawnForCategoryGlobal(c)) {
				categories.push_back(c);
			}
		}
		// ChunkMap.collectSpawningChunks, shuffled with the level's random (Util.shuffle)
		std::vector<Chunk*> chunks;
		level.forEachTickingChunk([&](Chunk& chunk) {
			if (level.anyPlayerCloseEnoughForSpawning({chunk.x() * 16, 0, chunk.z() * 16})) chunks.push_back(&chunk);
		});
		JavaRandom& random = level.random();
		for (int i = static_cast<int>(chunks.size()); i > 1; i--) std::swap(chunks[i - 1], chunks[random.nextInt(i)]);
		for (Chunk* chunk : chunks) {
			// The inhabited time and thunder are ticked by Level::tickChunks
			if (categories.empty()) continue;
			for (int category : categories) {
				if (!state.localMobCaps.canSpawn(category, chunk->x(), chunk->z())) continue;
				// spawnCategoryForChunk: a random position of the chunk, from the bottom up to the top block
				int x		= chunk->x() * 16 + random.nextInt(16);
				int z		= chunk->z() * 16 + random.nextInt(16);
				int surface = level.getHeight(Level::Heightmap::WorldSurface, x, z) + 1;
				int y		= random.nextInt(surface - level.minY() + 1) + level.minY(); // Mth.randomBetweenInclusive
				if (y >= level.minY() + 1) spawnCategoryForPosition(level, category, chunk->x(), chunk->z(), {x, y, z}, state);
			}
		}
	}
} // namespace NaturalSpawner
