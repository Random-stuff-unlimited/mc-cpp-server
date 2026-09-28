#include "LevelFixture.hpp"
#include "Test.hpp"

#include <cstdlib>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer

	std::string water(int level) { return "minecraft:water[level=" + std::to_string(level) + "]"; }
	std::string lava(int level) { return "minecraft:lava[level=" + std::to_string(level) + "]"; }
} // namespace

// A water source on flat ground: 7 blocks of flowing water around it, one level per block (a diamond), one block
// further every 5 ticks
TEST(fluids_water_spreads_like_vanilla) {
	LevelFixture f;
	f.set(0, Y, 0, water(0));
	f.tick(4);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:air");
	f.tick(1);
	CHECK(f.nameAt(1, Y, 0) == water(1));
	CHECK(f.nameAt(0, Y, -1) == water(1));
	CHECK(f.nameAt(2, Y, 0) == "minecraft:air");
	f.tick(60);
	for (int x = -9; x <= 9; x++) {
		for (int z = -9; z <= 9; z++) {
			int			distance = std::abs(x) + std::abs(z);
			std::string expected = distance <= 7 ? water(distance) : "minecraft:air";
			if (f.nameAt(x, Y, z) != expected) test::fail(__FILE__, __LINE__, "at " + std::to_string(x) + "," + std::to_string(z) + ": " + f.nameAt(x, Y, z));
		}
	}
	CHECK(f.nameAt(0, Y - 1, 0) == "minecraft:grass_block[snowy=false]");
}

// Lava in the overworld: 2 levels lost per block, 30 ticks per block
TEST(fluids_lava_spreads_like_vanilla) {
	LevelFixture f;
	f.set(0, Y, 0, lava(0));
	f.tick(29);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:air");
	f.tick(1);
	CHECK(f.nameAt(1, Y, 0) == lava(2));
	f.tick(200);
	for (int x = -5; x <= 5; x++) {
		for (int z = -5; z <= 5; z++) {
			int			distance = std::abs(x) + std::abs(z);
			std::string expected = distance <= 3 ? lava(2 * distance) : "minecraft:air";
			if (f.nameAt(x, Y, z) != expected) test::fail(__FILE__, __LINE__, "at " + std::to_string(x) + "," + std::to_string(z) + ": " + f.nameAt(x, Y, z));
		}
	}
}

// Two sources with a gap on solid ground fill it with a new source
TEST(fluids_infinite_source) {
	LevelFixture f;
	f.set(0, Y, 5, water(0));
	f.set(2, Y, 5, water(0));
	f.tick(20);
	CHECK(f.nameAt(1, Y, 5) == water(0));
}

// Water reaching a lava source turns it into obsidian
TEST(fluids_water_makes_obsidian) {
	LevelFixture f;
	f.set(20, Y, 0, lava(0));
	f.set(17, Y, 0, water(0));
	f.tick(20);
	CHECK(f.nameAt(20, Y, 0) == "minecraft:obsidian");
}

// A source in the air: its water falls straight down (falling water, level 8), then at its next tick the source
// spreads one block to each side (the water below can't be replaced), which falls too: vanilla's cross waterfall
TEST(fluids_water_falls) {
	LevelFixture f;
	f.set(0, Y + 3, 20, water(0));
	f.tick(5);
	CHECK(f.nameAt(0, Y + 2, 20) == water(8));
	CHECK(f.nameAt(1, Y + 3, 20) == "minecraft:air");
	f.tick(5);
	CHECK(f.nameAt(1, Y + 3, 20) == water(1));
	f.tick(40);
	CHECK(f.nameAt(0, Y, 20) == water(8));
	CHECK(f.nameAt(1, Y + 2, 20) == water(8));
	CHECK(f.nameAt(1, Y, 20) == water(8));
	CHECK(f.nameAt(2, Y + 3, 20) == "minecraft:air"); // Level 1 water falls instead of spreading
	CHECK(f.nameAt(2, Y, 20) == water(1));			   // Falling water spreads on the ground as level 7 (level=1)
}

// Without its source, flowing water dries up
TEST(fluids_dry_up_without_source) {
	LevelFixture f;
	f.set(30, Y, 30, water(0));
	f.tick(60);
	CHECK(f.nameAt(33, Y, 30) == water(3));
	f.set(30, Y, 30, "minecraft:air");
	f.tick(100);
	for (int x = 23; x <= 37; x++) {
		for (int z = 23; z <= 37; z++) {
			if (f.nameAt(x, Y, z) != "minecraft:air") test::fail(__FILE__, __LINE__, "left at " + std::to_string(x) + "," + std::to_string(z) + ": " + f.nameAt(x, Y, z));
		}
	}
}
