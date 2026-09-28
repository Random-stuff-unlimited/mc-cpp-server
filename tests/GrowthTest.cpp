#include "LevelFixture.hpp"
#include "Test.hpp"
#include "world/Chunk.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/blocks/Vegetation.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer

	// Random ticks on one block until it changes, at most `max`: how many it took, -1 if it never changed
	int randomTicksUntilChange(LevelFixture& f, int x, int y, int z, int max = 100000) {
		int state = f.at(x, y, z);
		for (int i = 1; i <= max; i++) {
			f.randomTick(x, y, z);
			if (f.at(x, y, z) != state) return i;
		}
		return -1;
	}
	void randomTicks(LevelFixture& f, int x, int y, int z, int count) {
		for (int i = 0; i < count; i++) f.randomTick(x, y, z);
	}
} // namespace

// Level.updateSkyBrightness: no darkening at noon, 11 at midnight
TEST(growth_sky_darken) {
	LevelFixture f(1);
	f.world->setDayTime(6000);
	f.level->updateSkyBrightness();
	CHECK_EQ(f.level->skyDarken(), 0);
	f.world->setDayTime(18000);
	f.level->updateSkyBrightness();
	CHECK_EQ(f.level->skyDarken(), 11);
	f.world->setDayTime(13000);
	f.level->updateSkyBrightness();
	CHECK(f.level->skyDarken() > 0 && f.level->skyDarken() < 11);
}

// Sections keep count of their randomly ticking blocks (grass here), and random ticks reach them
TEST(growth_random_tick_counts) {
	LevelFixture f(1);
	f.at(0, Y, 0); // The level takes the chunk
	std::shared_ptr<Chunk> chunk   = f.world->loadedChunk(0, 0);
	int					   surface = (LevelFixture::SURFACE - f.level->minY()) >> 4;
	CHECK_EQ(int(chunk->randomTickingCounts().at(surface)), 256); // A layer of grass
	f.set(0, Y + 20, 0, "minecraft:ice");
	CHECK_EQ(int(chunk->randomTickingCounts().at((Y + 20 - f.level->minY()) >> 4)), 1);
	f.set(0, Y + 20, 0, "minecraft:air");
	CHECK_EQ(int(chunk->randomTickingCounts().at((Y + 20 - f.level->minY()) >> 4)), 0);

	// Wheat grows with the world's random ticks alone
	f.set(0, Y - 1, 0, "minecraft:farmland[moisture=7]");
	f.set(0, Y, 0, "minecraft:wheat[age=0]");
	f.level->setRandomTickSpeed(3);
	f.tick(20); // Light
	f.level->setRandomTickSpeed(4096); // About once per tick for each block of the section
	f.tick(300);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:wheat[age=7]");
}

// CropBlock.getGrowthSpeed: 1, + 3 for its wet farmland, + 3/4 for each wet farmland around; halved by crops of
// the same kind in rows on both axes
TEST(growth_crop_speed) {
	LevelFixture f(1);
	BlockContext context(f.data);
	for (int x = -1; x <= 1; x++) {
		for (int z = -1; z <= 1; z++) f.set(x, Y - 1, z, "minecraft:farmland[moisture=7]");
	}
	int wheat	 = f.block("minecraft:wheat");
	int farmland = f.block("minecraft:farmland");
	CHECK(CropBlock::growthSpeed(context, *f.level, wheat, {0, Y, 0}, farmland) == 10.0F);
	f.set(1, Y, 0, "minecraft:wheat[age=0]");
	CHECK(CropBlock::growthSpeed(context, *f.level, wheat, {0, Y, 0}, farmland) == 10.0F);
	f.set(0, Y, 1, "minecraft:wheat[age=0]");
	CHECK(CropBlock::growthSpeed(context, *f.level, wheat, {0, Y, 0}, farmland) == 5.0F);
}

// Crops grow one age at a time, not past the last one; beetroots stop at 3
TEST(growth_crops) {
	LevelFixture f(1);
	f.set(0, Y - 1, 0, "minecraft:farmland[moisture=7]");
	f.set(0, Y, 0, "minecraft:wheat[age=0]");
	f.set(2, Y - 1, 0, "minecraft:farmland[moisture=7]");
	f.set(2, Y, 0, "minecraft:beetroots[age=0]");
	f.set(4, Y - 1, 0, "minecraft:farmland[moisture=7]");
	f.set(4, Y, 0, "minecraft:torchflower_crop[age=0]");
	f.tick(20); // Light
	randomTicks(f, 0, Y, 0, 2000);
	randomTicks(f, 2, Y, 0, 2000);
	for (int i = 0; i < 2000 && f.nameAt(4, Y, 0) != "minecraft:torchflower"; i++) f.randomTick(4, Y, 0);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:wheat[age=7]");
	CHECK(f.nameAt(2, Y, 0) == "minecraft:beetroots[age=3]");
	CHECK(f.nameAt(4, Y, 0) == "minecraft:torchflower");

	// In the dark: nothing
	LevelFixture dark(1);
	dark.set(0, Y - 1, 0, "minecraft:farmland[moisture=7]");
	dark.set(0, Y, 0, "minecraft:wheat[age=0]");
	dark.set(0, Y + 1, 0, "minecraft:stone");
	for (int x = -1; x <= 1; x++) {
		for (int z = -1; z <= 1; z++) {
			if (x != 0 || z != 0) dark.set(x, Y, z, "minecraft:stone");
		}
	}
	dark.tick(20);
	randomTicks(dark, 0, Y, 0, 500);
	CHECK(dark.nameAt(0, Y, 0) == "minecraft:wheat[age=0]");
}

// A grown stem puts its fruit on a free side with dirt below, and turns to it
TEST(growth_stem_fruit) {
	LevelFixture f(1);
	f.set(0, Y - 1, 0, "minecraft:farmland[moisture=7]");
	f.set(0, Y, 0, "minecraft:pumpkin_stem[age=7]");
	f.tick(20);
	CHECK(randomTicksUntilChange(f, 0, Y, 0) > 0);
	std::string stem = f.nameAt(0, Y, 0);
	CHECK(stem.rfind("minecraft:attached_pumpkin_stem[facing=", 0) == 0);
	int pumpkins = 0;
	for (auto [x, z] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) pumpkins += f.nameAt(x, Y, z) == "minecraft:pumpkin";
	CHECK_EQ(pumpkins, 1);
	// The pumpkin goes: the stem is a grown stem again
	for (auto [x, z] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
		if (f.nameAt(x, Y, z) == "minecraft:pumpkin") f.set(x, Y, z, "minecraft:air");
	}
	CHECK(f.nameAt(0, Y, 0) == "minecraft:pumpkin_stem[age=7]");
}

// Sugar cane and cactus: 16 random ticks per block, 3 blocks high at most
TEST(growth_sugar_cane_and_cactus) {
	LevelFixture f(1);
	f.set(0, Y - 1, 0, "minecraft:sand");
	f.set(1, Y - 1, 0, "minecraft:water[level=0]");
	f.set(0, Y, 0, "minecraft:sugar_cane[age=0]");
	randomTicks(f, 0, Y, 0, 15);
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:air");
	CHECK(f.nameAt(0, Y, 0) == "minecraft:sugar_cane[age=15]");
	f.randomTick(0, Y, 0);
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:sugar_cane[age=0]");
	CHECK(f.nameAt(0, Y, 0) == "minecraft:sugar_cane[age=0]");
	randomTicks(f, 0, Y + 1, 0, 16);
	randomTicks(f, 0, Y + 2, 0, 100);
	CHECK(f.nameAt(0, Y + 2, 0) == "minecraft:sugar_cane[age=0]");
	CHECK(f.nameAt(0, Y + 3, 0) == "minecraft:air");

	f.set(4, Y - 1, 4, "minecraft:sand");
	f.set(4, Y, 4, "minecraft:cactus[age=0]");
	for (int i = 0; i < 16; i++) f.randomTick(4, Y, 4);
	// Age 8 may give a flower instead (10%): either way something grew
	CHECK(f.nameAt(4, Y + 1, 4) == "minecraft:cactus[age=0]" || f.nameAt(4, Y + 1, 4) == "minecraft:cactus_flower");
}

// Kelp grows up to the water's surface; below the head it is kelp_plant
TEST(growth_kelp) {
	LevelFixture f(1);
	for (int y = Y; y < Y + 5; y++) {
		for (int x = -1; x <= 1; x++) {
			for (int z = -1; z <= 1; z++) f.set(x, y, z, (x == 0 && z == 0) ? "minecraft:water[level=0]" : "minecraft:stone");
		}
	}
	f.set(0, Y, 0, "minecraft:kelp[age=0]");
	f.tick(10);
	for (int i = 0; i < 2000; i++) {
		for (int y = Y; y < Y + 5; y++) {
			if (f.nameAt(0, y, 0).rfind("minecraft:kelp[", 0) == 0) f.randomTick(0, y, 0);
		}
		f.tick();
	}
	CHECK(f.nameAt(0, Y, 0) == "minecraft:kelp_plant");
	CHECK(f.nameAt(0, Y + 3, 0) == "minecraft:kelp_plant");
	CHECK(f.nameAt(0, Y + 4, 0).rfind("minecraft:kelp[", 0) == 0);
	CHECK(f.nameAt(0, Y + 5, 0) == "minecraft:air");
	// Cut in the middle: the top pieces break, the one below becomes a head
	f.level->destroyBlock({0, Y + 2, 0}, false);
	f.tick(5);
	CHECK(f.nameAt(0, Y + 1, 0).rfind("minecraft:kelp[", 0) == 0);
	CHECK(f.nameAt(0, Y + 3, 0) == "minecraft:water[level=0]");
}

// Grass spreads to dirt in the light; covered by a full block, it becomes dirt
TEST(growth_grass_spreads) {
	LevelFixture f(1);
	f.set(0, Y - 1, 0, "minecraft:dirt");
	f.set(1, Y - 1, 0, "minecraft:stone"); // Its own grass could spread too: gone
	f.tick(20);
	for (int i = 0; i < 2000 && f.nameAt(0, Y - 1, 0) == "minecraft:dirt"; i++) f.randomTick(-1, Y - 1, 0);
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:grass_block[snowy=false]");

	f.set(3, Y, 3, "minecraft:stone");
	f.randomTick(3, Y - 1, 3);
	CHECK(f.nameAt(3, Y - 1, 3) == "minecraft:dirt");
	// Slabs at the bottom close the top face too, glass doesn't
	f.set(5, Y, 5, "minecraft:glass");
	f.randomTick(5, Y - 1, 5);
	CHECK(f.nameAt(5, Y - 1, 5) == "minecraft:grass_block[snowy=false]");
	f.set(6, Y, 6, "minecraft:stone_slab[type=bottom,waterlogged=false]");
	f.randomTick(6, Y - 1, 6);
	CHECK(f.nameAt(6, Y - 1, 6) == "minecraft:dirt");
	// Snow on it: snowy
	f.set(8, Y, 8, "minecraft:snow[layers=1]");
	CHECK(f.nameAt(8, Y - 1, 8) == "minecraft:grass_block[snowy=true]");
}

// Farmland: wet near water, dries without it, then turns to dirt
TEST(growth_farmland) {
	LevelFixture f(1);
	f.set(0, Y - 1, 0, "minecraft:farmland[moisture=0]");
	f.set(4, Y - 1, 0, "minecraft:water[level=0]");
	f.randomTick(0, Y - 1, 0);
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:farmland[moisture=7]");
	f.set(4, Y - 1, 0, "minecraft:dirt");
	randomTicks(f, 0, Y - 1, 0, 7);
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:farmland[moisture=0]");
	f.randomTick(0, Y - 1, 0);
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:dirt");
	// A block on it: dirt the next tick
	f.set(2, Y - 1, 2, "minecraft:farmland[moisture=7]");
	f.set(2, Y, 2, "minecraft:stone");
	f.tick(1);
	CHECK(f.nameAt(2, Y - 1, 2) == "minecraft:dirt");
}

// Leaves take their distance to the log; without a log they decay and drop
TEST(growth_leaves_decay) {
	LevelFixture f(1);
	f.set(0, Y, 0, "minecraft:oak_log[axis=y]");
	f.set(1, Y, 0, "minecraft:oak_leaves[distance=7,persistent=false,waterlogged=false]");
	f.set(2, Y, 0, "minecraft:oak_leaves[distance=7,persistent=false,waterlogged=false]");
	f.tick(5);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:oak_leaves[distance=1,persistent=false,waterlogged=false]");
	CHECK(f.nameAt(2, Y, 0) == "minecraft:oak_leaves[distance=2,persistent=false,waterlogged=false]");
	f.set(0, Y, 0, "minecraft:air");
	f.tick(10);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:oak_leaves[distance=7,persistent=false,waterlogged=false]");
	f.randomTick(1, Y, 0);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:air");
}

// Ice melts next to a light; copper oxidizes
TEST(growth_ice_and_copper) {
	LevelFixture f(1);
	f.set(0, Y, 0, "minecraft:ice");
	f.set(1, Y, 0, "minecraft:glowstone");
	f.tick(1);
	f.randomTick(0, Y, 0);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:water[level=0]");

	f.set(4, Y, 4, "minecraft:copper_block");
	CHECK(randomTicksUntilChange(f, 4, Y, 4) > 0);
	CHECK(f.nameAt(4, Y, 4) == "minecraft:exposed_copper");
	// Far from the exposed copper, which would hold it back
	f.set(-6, Y, -6, "minecraft:weathered_cut_copper_stairs[facing=east,half=bottom,shape=straight,waterlogged=false]");
	CHECK(randomTicksUntilChange(f, -6, Y, -6) > 0);
	CHECK(f.nameAt(-6, Y, -6) == "minecraft:oxidized_cut_copper_stairs[facing=east,half=bottom,shape=straight,waterlogged=false]");
	// Next to less oxidized copper, it waits
	f.set(12, Y, 12, "minecraft:exposed_copper");
	f.set(13, Y, 12, "minecraft:copper_block");
	CHECK_EQ(randomTicksUntilChange(f, 12, Y, 12, 3000), -1);
}

namespace {
	// Grows the sapling(s) at x, z: random ticks until the block changes. Returns the number of logs placed
	int growTree(LevelFixture& f, int x, int z, const std::string& sapling, bool twoByTwo = false) {
		std::string stage1 = sapling + "[stage=1]";
		for (int dx = 0; dx <= (twoByTwo ? 1 : 0); dx++) {
			for (int dz = 0; dz <= (twoByTwo ? 1 : 0); dz++) f.set(x + dx, Y, z + dz, stage1);
		}
		f.tick(1);
		for (int i = 0; i < 5000 && f.nameAt(x, Y, z) == stage1; i++) f.randomTick(x, Y, z);
		f.tick(20); // The leaves' ticks
		int logs = 0;
		for (int dx = -12; dx <= 12; dx++) {
			for (int dy = 0; dy < 40; dy++) {
				for (int dz = -12; dz <= 12; dz++) logs += f.nameAt(x + dx, Y + dy, z + dz).find("_log[") != std::string::npos;
			}
		}
		return logs;
	}

	// Every leaf's distance is the real distance to the nearest log (after the tree's update of the distances and
	// the ticks it scheduled)
	int wrongLeaves(LevelFixture& f, int x, int z) {
		BlockContext c(f.data);
		int			 wrong = 0;
		for (int dx = -12; dx <= 12; dx++) {
			for (int dy = 0; dy < 40; dy++) {
				for (int dz = -12; dz <= 12; dz++) {
					BlockPos	pos{x + dx, Y + dy, z + dz};
					std::string name = f.nameAt(pos.x, pos.y, pos.z);
					if (name.find("_leaves[") == std::string::npos) continue;
					int distance = c.blocks.getInt(f.at(pos.x, pos.y, pos.z), c.blocks.property("distance"));
					int nearest	 = 7;
					// Leaves inside the crown get no tick after the tree's own update: vanilla leaves them as they are
					bool outside = false;
					for (Direction direction : Directions::ALL) {
						BlockPos next = pos.relative(direction);
						outside		  = outside || f.nameAt(next.x, next.y, next.z) == "minecraft:air";
					}
					if (!outside) continue;
					for (Direction direction : Directions::ALL) {
						BlockPos	next  = pos.relative(direction);
						std::string other = f.nameAt(next.x, next.y, next.z);
						if (other.find("_log[") != std::string::npos) nearest = 1;
						else if (other.find("_leaves[") != std::string::npos)
							nearest = std::min(nearest, c.blocks.getInt(f.at(next.x, next.y, next.z), c.blocks.property("distance")) + 1);
					}
					if (distance > nearest) wrong++;
				}
			}
		}
		return wrong;
	}
} // namespace

// Saplings become the trees of the game's data
TEST(growth_trees) {
	LevelFixture f(3);
	f.tick(1);
	int oak = growTree(f, 0, 0, "minecraft:oak_sapling");
	CHECK(oak >= 4);
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:dirt"); // The grass under the trunk
	CHECK(f.nameAt(0, Y, 0).rfind("minecraft:oak_log", 0) == 0);
	CHECK_EQ(wrongLeaves(f, 0, 0), 0);

	LevelFixture birch(3);
	CHECK(growTree(birch, 0, 0, "minecraft:birch_sapling") >= 5);
	LevelFixture spruce(3);
	CHECK(growTree(spruce, 0, 0, "minecraft:spruce_sapling") >= 5);
	CHECK_EQ(wrongLeaves(spruce, 0, 0), 0);
	LevelFixture acacia(3);
	CHECK(growTree(acacia, 0, 0, "minecraft:acacia_sapling") >= 5);
	LevelFixture cherry(3);
	CHECK(growTree(cherry, 0, 0, "minecraft:cherry_sapling") >= 5);
	LevelFixture darkOak(3);
	CHECK(growTree(darkOak, 0, 0, "minecraft:dark_oak_sapling", true) >= 24);
	LevelFixture megaSpruce(3);
	CHECK(growTree(megaSpruce, 0, 0, "minecraft:spruce_sapling", true) >= 4 * 12);
	LevelFixture jungle(3);
	CHECK(growTree(jungle, 0, 0, "minecraft:jungle_sapling", true) >= 4 * 9);
	// A lone dark oak sapling never grows
	LevelFixture lone(1);
	lone.set(0, Y, 0, "minecraft:dark_oak_sapling[stage=1]");
	lone.tick(1);
	randomTicks(lone, 0, Y, 0, 500);
	CHECK(lone.nameAt(0, Y, 0) == "minecraft:dark_oak_sapling[stage=1]");
	// No room: the sapling stays
	LevelFixture low(1);
	low.set(0, Y + 3, 0, "minecraft:stone");
	low.set(0, Y, 0, "minecraft:oak_sapling[stage=1]");
	low.tick(1);
	randomTicks(low, 0, Y, 0, 500);
	CHECK(low.nameAt(0, Y, 0) == "minecraft:oak_sapling[stage=1]");
}

// Fancy oaks (10% of oaks): many oaks until one is fancy (branches: logs lying along x or z)
TEST(growth_fancy_oak) {
	bool fancy = false;
	for (int attempt = 0; attempt < 60 && !fancy; attempt++) {
		LevelFixture f(3);
		f.tick(1);
		growTree(f, 0, 0, "minecraft:oak_sapling");
		for (int dx = -8; dx <= 8 && !fancy; dx++) {
			for (int dy = 0; dy < 20 && !fancy; dy++) {
				for (int dz = -8; dz <= 8 && !fancy; dz++) fancy = f.nameAt(dx, Y + dy, dz).find("oak_log[axis=x]") != std::string::npos ||
																   f.nameAt(dx, Y + dy, dz).find("oak_log[axis=z]") != std::string::npos;
			}
		}
		if (fancy) CHECK_EQ(wrongLeaves(f, 0, 0), 0);
	}
	CHECK(fancy);
}
