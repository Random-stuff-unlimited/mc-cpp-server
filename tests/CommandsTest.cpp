#include "Commands.hpp"
#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/item/Enchantments.hpp"

#include <cmath>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	std::shared_ptr<Player> makePlayer(LevelFixture& f) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(0.5, Y, 0.5);
		player->setOnGround(true);
		return player;
	}
} // namespace

TEST(commands_tp_absolute) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	Commands::run(f.server, *player, "tp 100 64 -200");
	CHECK_EQ(player->getX(), 100.0);
	CHECK_EQ(player->getY(), 64.0);
	CHECK_EQ(player->getZ(), -200.0);
	CHECK(player->isOnGround());
}

TEST(commands_tp_relative) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	Commands::run(f.server, *player, "tp ~5 ~ 10");
	CHECK_EQ(player->getX(), 5.5); // ~5: 0.5 + 5
	CHECK_EQ(player->getY(), Y);   // ~ alone keeps the current position
	CHECK_EQ(player->getZ(), 10.0); // 10: absolute
}

TEST(commands_tp_unknown_player) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	Commands::run(f.server, *player, "tp nobody 100 64 100"); // Unknown target: position is untouched
	CHECK_EQ(player->getX(), 0.5);
	CHECK_EQ(player->getY(), Y);
	CHECK_EQ(player->getZ(), 0.5);
}

TEST(commands_tp_bad_args) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	Commands::run(f.server, *player, "tp 100 64"); // Too few arguments: position is untouched
	CHECK_EQ(player->getX(), 0.5);
	Commands::run(f.server, *player, "tp abc def ghi"); // Not numbers
	CHECK_EQ(player->getX(), 0.5);
}

TEST(commands_unknown) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	Commands::run(f.server, *player, "not_a_command"); // Shows an error, nothing else
	CHECK_EQ(player->getX(), 0.5);
}
// /summon: a mob of that type where asked (relative coordinates), set up like vanilla's
TEST(commands_summon) {
	LevelFixture f(1);
	Commands::registerCommands();
	auto player = makePlayer(f);
	f.addPlayer(player);
	Commands::run(f.server, *player, "summon zombie ~3 ~ ~");
	Commands::run(f.server, *player, "summon minecraft:not_a_mob");
	int zombies = 0;
	double x	= 0.0;
	f.level->entities().forEach([&](Entity& entity) {
		if (entity.typeId() == f.data.getStaticId("minecraft:entity_type", "minecraft:zombie")) {
			zombies++;
			x = entity.position().x;
		}
	});
	CHECK_EQ(zombies, 1);
	CHECK_EQ(x, 3.5);
}

// /xp add and set, in points and levels
TEST(commands_xp) {
	LevelFixture f(1);
	Commands::registerCommands();
	auto player = makePlayer(f);
	f.addPlayer(player);
	Commands::run(f.server, *player, "xp add @s 5 levels");
	CHECK_EQ(player->getXpLevel(), 5);
	Commands::run(f.server, *player, "xp set @s 2 levels");
	CHECK_EQ(player->getXpLevel(), 2);
	Commands::run(f.server, *player, "experience add @s 100");
	CHECK(player->getXpLevel() > 2);
	Commands::run(f.server, *player, "xp set @s 0 levels");
	Commands::run(f.server, *player, "xp set @s 3 points");
	CHECK_EQ(player->getXpTotal(), 3);
}

// /enchant: the held sword gets sharpness; a conflicting or too high one is refused
TEST(commands_enchant) {
	LevelFixture f(1);
	Commands::registerCommands();
	auto player = makePlayer(f);
	f.addPlayer(player);
	player->setSelectedSlot(0);
	player->inventory().set(player->handSlot(0), ItemStack(f.item("minecraft:diamond_sword"), 1));
	Commands::run(f.server, *player, "enchant @s sharpness 3");
	auto enchantments = Enchantments::of(f.data, player->inventory().get(player->handSlot(0)));
	CHECK_EQ(static_cast<int>(enchantments.size()), 1);
	if (!enchantments.empty()) CHECK_EQ(enchantments[0].second, 3);
	Commands::run(f.server, *player, "enchant @s smite 1"); // Conflicts with sharpness
	Commands::run(f.server, *player, "enchant @s sharpness 9"); // Above its maximum
	CHECK_EQ(static_cast<int>(Enchantments::of(f.data, player->inventory().get(player->handSlot(0))).size()), 1);
}
