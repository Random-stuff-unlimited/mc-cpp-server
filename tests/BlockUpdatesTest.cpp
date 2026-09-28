#include "LevelFixture.hpp"
#include "Test.hpp"

namespace {
	constexpr int G = LevelFixture::SURFACE; // Grass
	constexpr int Y = G + 1;				   // First air layer
	const std::string AIR = "minecraft:air";
} // namespace

// Plants break at once when their ground goes
TEST(block_updates_plants_need_their_ground) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:dandelion");
	f.set(2, Y, 0, "minecraft:oak_sapling[stage=0]");
	f.set(0, G, 0, "minecraft:stone"); // Not dirt: the flower breaks
	CHECK(f.nameAt(0, Y, 0) == AIR);
	f.set(2, G, 0, "minecraft:dirt"); // Still dirt: it stays
	CHECK(f.nameAt(2, Y, 0) == "minecraft:oak_sapling[stage=0]");

	// Wheat needs farmland
	f.set(4, G, 0, "minecraft:farmland[moisture=7]");
	f.set(4, Y, 0, "minecraft:wheat[age=3]");
	CHECK(f.nameAt(4, Y, 0) == "minecraft:wheat[age=3]");
	f.set(4, G, 0, "minecraft:dirt");
	CHECK(f.nameAt(4, Y, 0) == AIR);
}

// Tall plants and doors: breaking either half removes the other one
TEST(block_updates_two_block_plants_and_doors) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:tall_grass[half=lower]");
	f.set(0, Y + 1, 0, "minecraft:tall_grass[half=upper]");
	f.set(0, Y + 1, 0, AIR);
	CHECK(f.nameAt(0, Y, 0) == AIR);

	std::string lower = "minecraft:oak_door[facing=north,half=lower,hinge=left,open=false,powered=false]";
	std::string upper = "minecraft:oak_door[facing=north,half=upper,hinge=left,open=false,powered=false]";
	f.set(2, Y, 0, lower);
	f.set(2, Y + 1, 0, upper);
	f.set(2, Y, 0, AIR);
	CHECK(f.nameAt(2, Y + 1, 0) == AIR);

	// The ground under a door goes: both halves break
	f.set(4, Y, 0, lower);
	f.set(4, Y + 1, 0, upper);
	f.set(4, G, 0, AIR);
	CHECK(f.nameAt(4, Y, 0) == AIR);
	CHECK(f.nameAt(4, Y + 1, 0) == AIR);
}

TEST(block_updates_bed_halves) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:red_bed[facing=north,occupied=false,part=foot]");
	f.set(0, Y, -1, "minecraft:red_bed[facing=north,occupied=false,part=head]"); // The head is where the foot faces
	f.set(0, Y, 0, AIR);
	CHECK(f.nameAt(0, Y, -1) == AIR);
}

// Torches, ladders, lanterns, carpets and snow lose their support
TEST(block_updates_attached_blocks) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:stone");
	f.set(0, Y + 1, 0, "minecraft:torch");
	f.set(1, Y, 0, "minecraft:wall_torch[facing=east]");	// On the east side of the stone
	f.set(0, Y, 1, "minecraft:ladder[facing=south,waterlogged=false]"); // South side
	f.set(0, Y, 0, AIR);
	CHECK(f.nameAt(0, Y + 1, 0) == AIR);
	CHECK(f.nameAt(1, Y, 0) == AIR);
	CHECK(f.nameAt(0, Y, 1) == AIR);

	f.set(5, Y + 2, 0, "minecraft:stone");
	f.set(5, Y + 1, 0, "minecraft:lantern[hanging=true,waterlogged=false]");
	f.set(5, Y + 2, 0, AIR);
	CHECK(f.nameAt(5, Y + 1, 0) == AIR);

	f.set(7, Y + 1, 0, "minecraft:stone");
	f.set(7, Y + 2, 0, "minecraft:white_carpet");
	f.set(7, Y + 3, 0, "minecraft:stone");
	f.set(7, Y + 4, 0, "minecraft:snow[layers=1]");
	f.set(7, Y + 1, 0, AIR);
	f.set(7, Y + 3, 0, AIR);
	CHECK(f.nameAt(7, Y + 2, 0) == AIR);
	CHECK(f.nameAt(7, Y + 4, 0) == AIR);
}

// Sugar cane checks its support in a scheduled tick: a column falls one block per tick
TEST(block_updates_sugar_cane_falls_tick_by_tick) {
	LevelFixture f;
	f.set(0, G, 0, "minecraft:water[level=0]");
	f.set(1, G, 0, "minecraft:sand");
	for (int y = Y; y < Y + 3; y++) f.set(1, y, 0, "minecraft:sugar_cane[age=0]");
	f.set(0, G, 0, "minecraft:stone"); // No water anymore, but the cane is diagonal to it: no update reaches it
	f.tick(5);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:sugar_cane[age=0]");
	f.set(1, G, 0, "minecraft:red_sand"); // Now it checks, and breaks at the next tick
	CHECK(f.nameAt(1, Y, 0) == "minecraft:sugar_cane[age=0]");
	f.tick(1);
	CHECK(f.nameAt(1, Y, 0) == AIR);
	CHECK(f.nameAt(1, Y + 1, 0) == "minecraft:sugar_cane[age=0]");
	f.tick(1);
	CHECK(f.nameAt(1, Y + 1, 0) == AIR);
	CHECK(f.nameAt(1, Y + 2, 0) == "minecraft:sugar_cane[age=0]");
	f.tick(1);
	CHECK(f.nameAt(1, Y + 2, 0) == AIR);
}

// A cactus can't stand next to a solid block
TEST(block_updates_cactus_next_to_block) {
	LevelFixture f;
	f.set(0, G, 0, "minecraft:sand");
	f.set(0, Y, 0, "minecraft:cactus[age=0]");
	f.set(1, Y, 0, "minecraft:stone");
	CHECK(f.nameAt(0, Y, 0) == "minecraft:cactus[age=0]");
	f.tick(1);
	CHECK(f.nameAt(0, Y, 0) == AIR);
}

// Without its pumpkin, an attached stem goes back to a grown stem
TEST(block_updates_attached_stem) {
	LevelFixture f;
	f.set(0, G, 0, "minecraft:farmland[moisture=7]");
	f.set(1, Y, 0, "minecraft:pumpkin");
	f.set(0, Y, 0, "minecraft:attached_pumpkin_stem[facing=east]");
	f.set(1, Y, 0, AIR);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:pumpkin_stem[age=7]");
}

// Water flowing into a flower washes it away; a waterloggable block isn't filled by flowing water
TEST(block_updates_water_breaks_plants) {
	LevelFixture f;
	f.set(1, Y, 0, "minecraft:poppy");
	f.set(0, Y, 0, "minecraft:water[level=0]");
	f.tick(5);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:water[level=1]");
}
