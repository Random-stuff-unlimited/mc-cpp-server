#include "world/entity/SpawnPlacements.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "player.hpp"
#include "world/Shapes.hpp"
#include "world/entity/DismountHelper.hpp"

#include <functional>
#include <unordered_map>

namespace SpawnPlacements {

	namespace {
		using Predicate = std::function<bool(Level&, int typeId, SpawnReason, const BlockPos&, JavaRandom&)>;
		using H			= Level::Heightmap;

		struct Data {
			H		  heightmap;
			Placement placement;
			Predicate predicate;
		};

		bool blockIs(Level& level, const BlockPos& pos, const char* block) {
			return level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(level.getBlockState(pos))) == block;
		}
		bool blockTagged(Level& level, const BlockPos& pos, const char* tag) {
			return level.gameData().isInTag("minecraft:block", tag, level.blocks().blockOf(level.getBlockState(pos)));
		}
		bool biomeTagged(Level& level, const BlockPos& pos, const char* tag) {
			return level.gameData().isInTag("minecraft:worldgen/biome", tag, level.getBiomeId(pos));
		}
		bool isWater(Level& level, const BlockPos& pos) { return level.fluids().isWater(level.getFluidState(pos).type); }
		bool isLava(Level& level, const BlockPos& pos) { return level.fluids().isLava(level.getFluidState(pos).type); }
		bool peaceful(Level& level) { return level.difficulty() == 0; }

		// Level.getNearestPlayer(x, y, z, distance, true): a player that isn't creative or a spectator within distance
		bool anyPlayerWithin(Level& level, double x, double y, double z, double distance) {
			for (const auto& player : level.players()) {
				if (player->isDisconnected() || player->isSpectator() || player->isCreative()) continue;
				double dx = player->getX() - x, dy = player->getY() - y, dz = player->getZ() - z;
				if (dx * dx + dy * dy + dz * dz < distance * distance) return true;
			}
			return false;
		}

		// WaterAnimal.checkSurfaceWaterAnimalSpawnRules (and AgeableWaterCreature's)
		bool surfaceWaterAnimal(Level& level, const BlockPos& pos) {
			int sea = level.seaLevel();
			return pos.y >= sea - 13 && pos.y <= sea && isWater(level, pos.below()) && blockIs(level, pos.above(), "minecraft:water");
		}

		// LevelReader.canSeeSkyFromBelowWater
		bool canSeeSkyFromBelowWater(Level& level, const BlockPos& pos) {
			int sea = level.seaLevel();
			if (pos.y >= sea) return level.canSeeSky(pos);
			BlockPos top{pos.x, sea, pos.z};
			if (!level.canSeeSky(top)) return false;
			for (BlockPos p = top.below(); p.y > pos.y; p = p.below()) {
				int								   state = level.getBlockState(p);
				const GameData::StateProperties& props = level.gameData().getStateProperties(state);
				if (props.lightBlock > 0 && !props.liquid) return false;
			}
			return true;
		}

		bool monsterRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			return checkMonsterSpawnRules(level, type, reason, pos, random);
		}
		bool anyLightMonsterRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom&) {
			return checkAnyLightMonsterSpawnRules(level, type, reason, pos);
		}
		bool mobRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom&) { return checkMobSpawnRules(level, type, reason, pos); }
		bool animalRules(Level& level, int, SpawnReason reason, const BlockPos& pos, JavaRandom&) { return checkAnimalSpawnRules(level, reason, pos); }
		// The animals spawning on their own blocks, in the light (armadillo, camel, frog, goat, mooshroom, parrot, rabbit,
		// wolf, fox)
		Predicate onBlocksInLight(const char* tag) {
			return [tag](Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) {
				return blockTagged(level, pos.below(), tag) && isBrightEnoughToSpawn(level, pos);
			};
		}
		bool surfaceWaterRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) { return surfaceWaterAnimal(level, pos); }
		// Silverfish and endermites: not right next to a player
		bool infestationRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom&) {
			if (!checkAnyLightMonsterSpawnRules(level, type, reason, pos)) return false;
			if (MobRegistry::isSpawner(reason)) return true;
			return !anyPlayerWithin(level, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, 5.0);
		}
		// SkeletonHorse and ZombieHorse
		bool undeadHorseRules(Level& level, int, SpawnReason reason, const BlockPos& pos, JavaRandom&) {
			if (!MobRegistry::isSpawner(reason)) return checkAnimalSpawnRules(level, reason, pos);
			return MobRegistry::ignoresLightRequirements(reason) || isBrightEnoughToSpawn(level, pos);
		}
		bool notOnNetherWart(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) { return !blockIs(level, pos.below(), "minecraft:nether_wart_block"); }

		bool drownedRules(Level& level, int, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			bool spawner = MobRegistry::isSpawner(reason);
			if (!isWater(level, pos.below()) && !spawner) return false;
			bool ok = !peaceful(level) && (MobRegistry::ignoresLightRequirements(reason) || isDarkEnoughToSpawn(level, pos, random)) &&
					  (spawner || isWater(level, pos));
			if (ok && (spawner || reason == SpawnReason::Reinforcement)) return true;
			if (biomeTagged(level, pos, "minecraft:more_frequent_drowned_spawns")) return random.nextInt(15) == 0 && ok;
			return random.nextInt(40) == 0 && pos.y < level.seaLevel() - 5 && ok; // isDeepEnoughToSpawn
		}
		bool guardianRules(Level& level, int, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			return (random.nextInt(20) == 0 || !canSeeSkyFromBelowWater(level, pos)) && !peaceful(level) &&
				   (MobRegistry::isSpawner(reason) || isWater(level, pos)) && isWater(level, pos.below());
		}
		bool tropicalFishRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) {
			return isWater(level, pos.below()) && blockIs(level, pos.above(), "minecraft:water") &&
				   (biomeTagged(level, pos, "minecraft:allows_tropical_fish_spawns_at_any_height") || surfaceWaterAnimal(level, pos));
		}
		bool axolotlRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) { return blockTagged(level, pos.below(), "minecraft:axolotls_spawnable_on"); }
		bool glowSquidRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) {
			return pos.y <= level.seaLevel() - 33 && level.getRawBrightness(pos, 0) == 0 && blockIs(level, pos, "minecraft:water");
		}
		bool batRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			if (pos.y >= level.getHeight(H::WorldSurface, pos.x, pos.z)) return false;
			int brightness = level.getMaxLocalRawBrightness(pos);
			int limit	   = 4;
			// Bat.isHalloween: October 20 to November 3
			std::time_t now = std::time(nullptr);
			std::tm		date{};
			localtime_r(&now, &date);
			int day = date.tm_mday, month = date.tm_mon + 1;
			if ((month == 10 && day >= 20) || (month == 11 && day <= 3)) {
				limit = 7;
			} else if (random.nextBoolean()) {
				return false;
			}
			if (brightness > random.nextInt(limit)) return false;
			return blockTagged(level, pos.below(), "minecraft:bats_spawnable_on") && checkMobSpawnRules(level, type, reason, pos);
		}
		bool ghastRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			return !peaceful(level) && random.nextInt(20) == 0 && checkMobSpawnRules(level, type, reason, pos);
		}
		bool magmaCubeRules(Level& level, int, SpawnReason, const BlockPos&, JavaRandom&) { return !peaceful(level); }
		bool ocelotRules(Level&, int, SpawnReason, const BlockPos&, JavaRandom& random) { return random.nextInt(3) != 0; }
		bool patrollingRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom&) {
			return level.getBlockLight(pos) > 8 ? false : checkAnyLightMonsterSpawnRules(level, type, reason, pos);
		}
		bool polarBearRules(Level& level, int, SpawnReason reason, const BlockPos& pos, JavaRandom&) {
			if (!biomeTagged(level, pos, "minecraft:polar_bears_spawn_on_alternate_blocks")) return checkAnimalSpawnRules(level, reason, pos);
			return isBrightEnoughToSpawn(level, pos) && blockTagged(level, pos.below(), "minecraft:polar_bears_spawnable_on_alternate");
		}
		bool slimeRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			if (peaceful(level)) return false;
			if (MobRegistry::isSpawner(reason)) return checkMobSpawnRules(level, type, reason, pos);
			if (biomeTagged(level, pos, "minecraft:allows_surface_slime_spawns") && pos.y > 50 && pos.y < 70 && random.nextFloat() < 0.5F &&
				random.nextFloat() < level.getMoonBrightness() && level.getMaxLocalRawBrightness(pos) <= random.nextInt(8)) {
				return checkMobSpawnRules(level, type, reason, pos);
			}
			// The level is a WorldGenLevel on the server
			bool slimeChunk = isSlimeChunk(level.world().getSeed(), pos.chunkX(), pos.chunkZ());
			if (random.nextInt(10) == 0 && slimeChunk && pos.y < 40) return checkMobSpawnRules(level, type, reason, pos);
			return false;
		}
		bool strayRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			BlockPos above = pos;
			do {
				above = above.above();
			} while (blockIs(level, above, "minecraft:powder_snow"));
			return checkMonsterSpawnRules(level, type, reason, pos, random) && (MobRegistry::isSpawner(reason) || level.canSeeSky(above.below()));
		}
		bool huskRules(Level& level, int type, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
			return checkMonsterSpawnRules(level, type, reason, pos, random) && (MobRegistry::isSpawner(reason) || level.canSeeSky(pos));
		}
		bool striderRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) {
			BlockPos above = pos;
			do {
				above = above.above();
			} while (isLava(level, above));
			return level.blocks().isAir(level.getBlockState(above));
		}
		bool turtleRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) {
			// TurtleEggBlock.onSand: the block below is sand
			return pos.y < level.seaLevel() + 4 && blockTagged(level, pos.below(), "minecraft:sand") && isBrightEnoughToSpawn(level, pos);
		}
		bool zombifiedPiglinRules(Level& level, int, SpawnReason, const BlockPos& pos, JavaRandom&) {
			return !peaceful(level) && !blockIs(level, pos.below(), "minecraft:nether_wart_block");
		}

		// SpawnPlacements' static table
		const std::unordered_map<int, Data>& table(const GameData& data) {
			static const GameData*				  built = nullptr;
			static std::unordered_map<int, Data> byType;
			if (built == &data) return byType;
			built = &data;
			byType.clear();
			auto reg = [&](const char* type, Placement placement, H heightmap, Predicate predicate) {
				int id = data.getStaticId("minecraft:entity_type", std::string("minecraft:") + type);
				if (id >= 0) byType[id] = {heightmap, placement, std::move(predicate)};
			};
			const H NL = H::MotionBlockingNoLeaves, MB = H::MotionBlocking;
			const Placement W = Placement::InWater, G = Placement::OnGround, N = Placement::NoRestrictions;
			reg("axolotl", W, NL, axolotlRules);
			reg("cod", W, NL, surfaceWaterRules);
			reg("dolphin", W, NL, surfaceWaterRules);
			reg("drowned", W, NL, drownedRules);
			reg("guardian", W, NL, guardianRules);
			reg("pufferfish", W, NL, surfaceWaterRules);
			reg("salmon", W, NL, surfaceWaterRules);
			reg("squid", W, NL, surfaceWaterRules);
			reg("tropical_fish", W, NL, tropicalFishRules);
			reg("armadillo", G, NL, onBlocksInLight("minecraft:armadillo_spawnable_on"));
			reg("bat", G, NL, batRules);
			reg("blaze", G, NL, anyLightMonsterRules);
			reg("bogged", G, NL, monsterRules);
			reg("breeze", G, NL, anyLightMonsterRules);
			reg("camel", G, NL, onBlocksInLight("minecraft:camels_spawnable_on"));
			reg("cave_spider", G, NL, monsterRules);
			reg("chicken", G, NL, animalRules);
			reg("cow", G, NL, animalRules);
			reg("creeper", G, NL, monsterRules);
			reg("donkey", G, NL, animalRules);
			reg("enderman", G, NL, monsterRules);
			reg("endermite", G, NL, infestationRules);
			reg("ender_dragon", G, NL, mobRules);
			reg("frog", G, NL, onBlocksInLight("minecraft:frogs_spawnable_on"));
			reg("ghast", G, NL, ghastRules);
			reg("happy_ghast", G, NL, animalRules);
			reg("giant", G, NL, monsterRules);
			reg("glow_squid", W, NL, glowSquidRules);
			reg("goat", G, NL, onBlocksInLight("minecraft:goats_spawnable_on"));
			reg("horse", G, NL, animalRules);
			reg("husk", G, NL, huskRules);
			reg("iron_golem", G, NL, mobRules);
			reg("llama", G, NL, animalRules);
			reg("magma_cube", G, NL, magmaCubeRules);
			reg("mooshroom", G, NL, onBlocksInLight("minecraft:mooshrooms_spawnable_on"));
			reg("mule", G, NL, animalRules);
			reg("ocelot", G, MB, ocelotRules);
			reg("parrot", G, MB, onBlocksInLight("minecraft:parrots_spawnable_on"));
			reg("pig", G, NL, animalRules);
			reg("hoglin", G, NL, notOnNetherWart);
			reg("piglin", G, NL, notOnNetherWart);
			reg("pillager", G, NL, patrollingRules);
			reg("polar_bear", G, NL, polarBearRules);
			reg("rabbit", G, NL, onBlocksInLight("minecraft:rabbits_spawnable_on"));
			reg("sheep", G, NL, animalRules);
			reg("silverfish", G, NL, infestationRules);
			reg("skeleton", G, NL, monsterRules);
			reg("skeleton_horse", G, NL, undeadHorseRules);
			reg("slime", G, NL, slimeRules);
			reg("snow_golem", G, NL, mobRules);
			reg("spider", G, NL, monsterRules);
			reg("stray", G, NL, strayRules);
			reg("strider", Placement::InLava, NL, striderRules);
			reg("turtle", G, NL, turtleRules);
			reg("villager", G, NL, mobRules);
			reg("witch", G, NL, monsterRules);
			reg("wither", G, NL, monsterRules);
			reg("wither_skeleton", G, NL, monsterRules);
			reg("wolf", G, NL, onBlocksInLight("minecraft:wolves_spawnable_on"));
			reg("zoglin", G, NL, anyLightMonsterRules);
			reg("creaking", G, NL, monsterRules);
			reg("zombie", G, NL, monsterRules);
			reg("zombie_horse", G, NL, undeadHorseRules);
			reg("zombified_piglin", G, NL, zombifiedPiglinRules);
			reg("zombie_villager", G, NL, monsterRules);
			reg("cat", G, NL, animalRules);
			reg("elder_guardian", W, NL, guardianRules);
			reg("evoker", N, NL, monsterRules);
			reg("fox", N, NL, onBlocksInLight("minecraft:foxes_spawnable_on"));
			reg("illusioner", N, NL, monsterRules);
			reg("panda", N, NL, animalRules);
			reg("phantom", N, NL, mobRules);
			reg("ravager", G, NL, monsterRules);
			reg("shulker", N, NL, mobRules);
			reg("trader_llama", N, NL, animalRules);
			reg("vex", N, NL, monsterRules);
			reg("vindicator", N, NL, monsterRules);
			reg("wandering_trader", G, NL, mobRules);
			reg("warden", N, NL, monsterRules);
			return byType;
		}

		const Data* dataOf(Level& level, int typeId) {
			const auto& t  = table(level.gameData());
			auto		it = t.find(typeId);
			return it == t.end() ? nullptr : &it->second;
		}
	} // namespace

	Placement placementType(Level& level, int typeId) {
		const Data* data = dataOf(level, typeId);
		return data ? data->placement : Placement::NoRestrictions;
	}

	Level::Heightmap heightmapType(Level& level, int typeId) {
		const Data* data = dataOf(level, typeId);
		return data ? data->heightmap : H::MotionBlockingNoLeaves;
	}

	bool isSpawnPositionOk(Level& level, int typeId, const BlockPos& pos) {
		// The world border isn't ported: every position is within it
		switch (placementType(level, typeId)) {
		case Placement::NoRestrictions: return true;
		case Placement::InWater: return isWater(level, pos) && !level.isRedstoneConductor(level.getBlockState(pos.above()));
		case Placement::InLava: return isLava(level, pos);
		case Placement::OnGround: {
			if (!isValidSpawn(level, level.getBlockState(pos.below()), typeId)) return false;
			return isValidEmptySpawnBlock(level, pos, level.getBlockState(pos), typeId) &&
				   isValidEmptySpawnBlock(level, pos.above(), level.getBlockState(pos.above()), typeId);
		}
		}
		return false;
	}

	BlockPos adjustSpawnPosition(Level& level, Placement placement, const BlockPos& pos) {
		if (placement != Placement::OnGround) return pos;
		BlockPos below = pos.below();
		return (level.gameData().getStateProperties(level.getBlockState(below)).pathfindable & 1) ? below : pos;
	}

	bool checkSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
		const Data* data = dataOf(level, typeId);
		return !data || data->predicate(level, typeId, reason, pos, random);
	}

	bool isValidSpawn(Level& level, int state, int typeId) {
		uint8_t							bits = level.gameData().getStateProperties(state).validSpawn;
		const std::string&				name = level.gameData().getStaticName("minecraft:entity_type", typeId);
		const GameData::EntityTypeInfo* type = level.gameData().getEntityType(typeId);
		using SP							 = GameData::StateProperties;
		if (name == "minecraft:ocelot") return bits & SP::VALID_SPAWN_OCELOT;
		if (name == "minecraft:parrot") return bits & SP::VALID_SPAWN_PARROT;
		if (name == "minecraft:polar_bear") return bits & SP::VALID_SPAWN_POLAR_BEAR;
		if (type && type->fireImmune) return bits & SP::VALID_SPAWN_FIRE_IMMUNE;
		return bits & SP::VALID_SPAWN_MOB;
	}

	bool isValidEmptySpawnBlock(Level& level, const BlockPos& pos, int state, int typeId) {
		const GameData& data = level.gameData();
		if (Shapes::isFullBlock(data.getCollisionShape(state))) return false;
		if (level.isSignalSource(state)) return false;
		if (level.fluids().stateOf(state).type != 0) return false;
		if (data.isInTag("minecraft:block", "minecraft:prevent_mob_spawning_inside", level.blocks().blockOf(state))) return false;
		(void)pos;
		return !isBlockDangerous(level, state, typeId);
	}

	bool isBlockDangerous(Level& level, int state, int typeId) {
		const GameData&	   data	 = level.gameData();
		const std::string& type	 = data.getStaticName("minecraft:entity_type", typeId);
		const std::string& block = data.getStaticName("minecraft:block", level.blocks().blockOf(state));
		// EntityType.immuneTo
		if (block == "minecraft:sweet_berry_bush" && type == "minecraft:fox") return false;
		if (block == "minecraft:powder_snow" && (type == "minecraft:polar_bear" || type == "minecraft:snow_golem" || type == "minecraft:stray")) return false;
		if (block == "minecraft:wither_rose" && (type == "minecraft:wither" || type == "minecraft:wither_skeleton")) return false;
		const GameData::EntityTypeInfo* info = data.getEntityType(typeId);
		return DismountHelper::isBlockDangerous(level, state, info && info->fireImmune);
	}

	bool isDarkEnoughToSpawn(Level& level, const BlockPos& pos, JavaRandom& random) {
		if (level.getSkyLight(pos) > random.nextInt(32)) return false;
		const GameData::Dimension& dim	 = level.dimensionType();
		int						   limit = dim.monsterSpawnBlockLightLimit;
		if (limit < 15 && level.getBlockLight(pos) > limit) return false;
		int light = level.isThundering() ? level.getRawBrightness(pos, 10) : level.getMaxLocalRawBrightness(pos);
		// monsterSpawnLightTest.sample: a constant, or uniform between its bounds
		int sample = dim.monsterSpawnLightMin == dim.monsterSpawnLightMax
						 ? dim.monsterSpawnLightMin
						 : random.nextInt(dim.monsterSpawnLightMax - dim.monsterSpawnLightMin + 1) + dim.monsterSpawnLightMin;
		return light <= sample;
	}

	bool checkMobSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos) {
		BlockPos below = pos.below();
		return MobRegistry::isSpawner(reason) || isValidSpawn(level, level.getBlockState(below), typeId);
	}

	bool checkMonsterSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos, JavaRandom& random) {
		return !peaceful(level) && (MobRegistry::ignoresLightRequirements(reason) || isDarkEnoughToSpawn(level, pos, random)) &&
			   checkMobSpawnRules(level, typeId, reason, pos);
	}

	bool checkAnyLightMonsterSpawnRules(Level& level, int typeId, SpawnReason reason, const BlockPos& pos) {
		return !peaceful(level) && checkMobSpawnRules(level, typeId, reason, pos);
	}

	bool checkAnimalSpawnRules(Level& level, SpawnReason reason, const BlockPos& pos) {
		bool light = MobRegistry::ignoresLightRequirements(reason) || isBrightEnoughToSpawn(level, pos);
		return blockTagged(level, pos.below(), "minecraft:animals_spawnable_on") && light;
	}

	bool isBrightEnoughToSpawn(Level& level, const BlockPos& pos) { return level.getRawBrightness(pos, 0) > 8; }

	bool containsAnyLiquid(Level& level, const AABB& box) {
		for (int x = Mth::floor(box.minX); x < Mth::ceil(box.maxX); x++) {
			for (int y = Mth::floor(box.minY); y < Mth::ceil(box.maxY); y++) {
				for (int z = Mth::floor(box.minZ); z < Mth::ceil(box.maxZ); z++) {
					if (level.getFluidState({x, y, z}).type != 0) return true;
				}
			}
		}
		return false;
	}

	bool isSlimeChunk(int64_t seed, int chunkX, int chunkZ) {
		// Java int arithmetic where vanilla's is int (x * x * 4987142, x * 5947611, z * 389711), long for z * z * 4392871L
		int32_t a = static_cast<int32_t>(static_cast<uint32_t>(chunkX) * static_cast<uint32_t>(chunkX) * 4987142u);
		int32_t b = static_cast<int32_t>(static_cast<uint32_t>(chunkX) * 5947611u);
		int64_t c = static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(chunkZ) * static_cast<uint32_t>(chunkZ))) * 4392871LL;
		int32_t d = static_cast<int32_t>(static_cast<uint32_t>(chunkZ) * 389711u);
		int64_t s = static_cast<int64_t>(static_cast<uint64_t>(seed) + static_cast<uint64_t>(static_cast<int64_t>(a)) + static_cast<uint64_t>(static_cast<int64_t>(b)) +
										 static_cast<uint64_t>(c) + static_cast<uint64_t>(static_cast<int64_t>(d)));
		JavaRandom random(s ^ 987234911LL);
		return random.nextInt(10) == 0;
	}
} // namespace SpawnPlacements
