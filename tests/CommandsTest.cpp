#include "Commands.hpp"
#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"

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