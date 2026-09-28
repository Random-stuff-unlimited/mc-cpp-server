#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	void use(LevelFixture& f, Player& player, int x, int y, int z) {
		int state = f.at(x, y, z);
		f.level->behavior(state).useWithoutItem(*f.level, {x, y, z}, state, player);
	}
	const char* PISTON		  = "minecraft:piston[extended=false,facing=east]";
	const char* STICKY_PISTON = "minecraft:sticky_piston[extended=false,facing=east]";
	const char* LEVER		  = "minecraft:lever[face=floor,facing=north,powered=false]";
} // namespace

// A piston pushes the block in front of it; the move takes 2 ticks after the block event
TEST(piston_extends_and_retracts) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	f.set(0, Y, 0, PISTON);
	f.set(1, Y, 0, "minecraft:stone");
	f.set(0, Y, -1, LEVER);
	f.tick(1);
	use(f, *player, 0, Y, -1);
	f.tick(1); // Block event at the end of the tick: moving blocks
	CHECK(f.nameAt(0, Y, 0) == "minecraft:piston[extended=true,facing=east]");
	CHECK(f.nameAt(1, Y, 0).rfind("minecraft:moving_piston", 0) == 0);
	CHECK(f.nameAt(2, Y, 0).rfind("minecraft:moving_piston", 0) == 0);
	f.tick(3);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:piston_head[facing=east,short=false,type=normal]");
	CHECK(f.nameAt(2, Y, 0) == "minecraft:stone");

	// Off: the head goes back, the stone stays
	use(f, *player, 0, Y, -1);
	f.tick(4);
	CHECK(f.nameAt(0, Y, 0) == PISTON);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:air");
	CHECK(f.nameAt(2, Y, 0) == "minecraft:stone");
}

// A sticky piston pulls its block back
TEST(piston_sticky_pulls) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	f.set(0, Y, 0, STICKY_PISTON);
	f.set(1, Y, 0, "minecraft:stone");
	f.set(0, Y, -1, LEVER);
	f.tick(1);
	use(f, *player, 0, Y, -1);
	f.tick(4);
	CHECK(f.nameAt(2, Y, 0) == "minecraft:stone");
	use(f, *player, 0, Y, -1);
	f.tick(4);
	CHECK(f.nameAt(0, Y, 0) == STICKY_PISTON);
	CHECK(f.nameAt(1, Y, 0) == "minecraft:stone");
	CHECK(f.nameAt(2, Y, 0) == "minecraft:air");
}

// Limits: 12 blocks at most, obsidian never moves; flowers in the way break
TEST(piston_limits) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	f.set(0, Y, 0, PISTON);
	for (int x = 1; x <= 13; x++) f.set(x, Y, 0, "minecraft:stone");
	f.set(0, Y, -1, LEVER);
	f.set(0, Y, 3, PISTON);
	f.set(1, Y, 3, "minecraft:obsidian");
	f.set(0, Y, 2, LEVER);
	f.set(0, Y, 6, PISTON);
	f.set(1, Y, 6, "minecraft:stone");
	f.set(2, Y, 6, "minecraft:poppy");
	f.set(0, Y, 5, LEVER);
	f.tick(1);
	use(f, *player, 0, Y, -1);
	use(f, *player, 0, Y, 2);
	use(f, *player, 0, Y, 5);
	f.tick(4);
	CHECK(f.nameAt(0, Y, 0) == PISTON); // 13 blocks: too many
	CHECK(f.nameAt(0, Y, 3) == PISTON);
	CHECK(f.nameAt(3, Y, 6) == "minecraft:air");
	CHECK(f.nameAt(2, Y, 6) == "minecraft:stone");
	bool poppy = false;
	for (ItemEntity* item : f.items()) poppy = poppy || f.data.getStaticName("minecraft:item", item->item().item) == "minecraft:poppy";
	CHECK(poppy);
	f.set(13, Y, 0, "minecraft:air"); // 12 now: it pushes
	f.tick(1);
	use(f, *player, 0, Y, -1);
	use(f, *player, 0, Y, -1);
	f.tick(4);
	CHECK(f.nameAt(13, Y, 0) == "minecraft:stone");
}

// Slime pulls the blocks stuck to it along (in the air: on the ground it would drag the whole ground, too many)
TEST(piston_slime) {
	LevelFixture f;
	int			 y = Y + 3;
	f.set(1, y, 0, "minecraft:slime_block");
	f.set(1, y + 1, 0, "minecraft:stone");
	f.set(1, y, 1, "minecraft:dirt");
	f.set(0, y, 0, PISTON);
	f.tick(1);
	f.set(0, y, -1, "minecraft:redstone_block");
	f.tick(4);
	CHECK(f.nameAt(2, y, 0) == "minecraft:slime_block");
	CHECK(f.nameAt(2, y + 1, 0) == "minecraft:stone");
	CHECK(f.nameAt(2, y, 1) == "minecraft:dirt");
	CHECK(f.nameAt(1, y + 1, 0) == "minecraft:air");
	CHECK(f.nameAt(1, y, 0) == "minecraft:piston_head[facing=east,short=false,type=normal]");

	// On the ground, the slime would drag the ground along: too many blocks, nothing moves
	LevelFixture g;
	g.set(1, Y, 0, "minecraft:slime_block");
	g.set(0, Y, 0, PISTON);
	g.set(0, Y, -1, "minecraft:redstone_block");
	g.tick(4);
	CHECK(g.nameAt(1, Y, 0) == "minecraft:slime_block");
}

// Items in the way are pushed
TEST(piston_pushes_items) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	f.set(0, Y, 0, PISTON);
	f.set(0, Y, -1, LEVER);
	f.tick(1);
	f.level->entities().add(std::make_unique<ItemEntity>(*f.level, Vec3{1.5, static_cast<double>(Y), 0.5}, ItemStack(f.item("minecraft:dirt"), 1), Vec3{}));
	f.tick(2);
	use(f, *player, 0, Y, -1);
	f.tick(4);
	auto items = f.items();
	CHECK_EQ(items.size(), size_t(1));
	if (!items.empty()) CHECK(items[0]->position().x > 2.0);
}
