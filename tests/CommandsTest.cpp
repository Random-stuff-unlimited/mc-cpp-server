#include "Commands.hpp"
#include "commands/CommandSupport.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/item/Enchantments.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	std::shared_ptr<Player> makePlayer(LevelFixture& f) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(0.5, Y, 0.5);
		player->setOnGround(true);
		player->setPermissionLevel(4); // An op: every command
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

// Permission levels: a player that isn't an op only has the level 0 commands (help, list, tell, me...)
TEST(commands_need_permission) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	f.addPlayer(player);
	player->setPermissionLevel(0);
	Commands::run(f.server, *player, "tp 100 64 -200"); // Unknown for it: nothing happens
	CHECK(player->getX() == 0.5);
	const Commands::Command* tell = nullptr;
	const Commands::Command* op	  = nullptr;
	for (const auto& command : Commands::commandList()) {
		if (command.name == "tell") tell = &command;
		if (command.name == "op") op = &command;
	}
	CHECK(tell && Commands::canUse(*player, *tell));
	CHECK(op && !Commands::canUse(*player, *op));
	player->setPermissionLevel(3);
	CHECK(Commands::canUse(*player, *op));
	Commands::run(f.server, *player, "tp 100 64 -200");
	CHECK(player->getX() == 100.0);
}

// /op and /deop: the level comes from the ops list, online players get theirs right away
TEST(commands_op_deop) {
	LevelFixture f;
	Commands::registerCommands();
	auto admin = makePlayer(f);
	f.addPlayer(admin);
	auto alice = std::make_shared<Player>("Alice", PlayerState::Play, -1, f.server);
	alice->setUUID(UUID::fromOfflinePlayer("Alice"));
	f.addPlayer(alice);
	CHECK_EQ(alice->permissionLevel(), 0);
	Commands::run(f.server, *admin, "op Alice");
	CHECK_EQ(f.server.access().permissionLevel(UUID::fromOfflinePlayer("Alice")), 4);
	CHECK_EQ(alice->permissionLevel(), 4);
	Commands::run(f.server, *admin, "op Carol"); // Offline: still made an op, by its offline UUID
	CHECK_EQ(f.server.access().permissionLevel(UUID::fromOfflinePlayer("Carol")), 4);
	Commands::run(f.server, *admin, "deop Alice");
	CHECK_EQ(alice->permissionLevel(), 0);
	CHECK_EQ(f.server.access().ops().size(), 1u);
}

// Bans (by name, by IP) and the whitelist refuse logins; /pardon lifts a ban; ops pass the whitelist
TEST(commands_ban_pardon_whitelist) {
	LevelFixture f;
	Commands::registerCommands();
	auto admin = makePlayer(f);
	f.addPlayer(admin);
	ServerAccess& access = f.server.access();
	UUID		  mallory = UUID::fromOfflinePlayer("Mallory");
	Commands::run(f.server, *admin, "ban Mallory griefing the spawn");
	auto refusal = access.loginRefusal(mallory, "10.0.0.5");
	CHECK(refusal && refusal->key == "multiplayer.disconnect.banned.reason" && refusal->args.at(0) == "griefing the spawn");
	CHECK(access.bans().at(0).source == "Bob");
	Commands::run(f.server, *admin, "pardon mallory");
	CHECK(!access.loginRefusal(mallory, "10.0.0.5"));
	Commands::run(f.server, *admin, "ban-ip 10.0.0.5");
	refusal = access.loginRefusal(mallory, "10.0.0.5");
	CHECK(refusal && refusal->key == "multiplayer.disconnect.banned_ip.reason");
	CHECK(!access.loginRefusal(mallory, "10.0.0.6"));
	Commands::run(f.server, *admin, "ban-ip not.an.ip"); // Neither an IP nor an online player
	CHECK_EQ(access.ipBans().size(), 1u);
	Commands::run(f.server, *admin, "pardon-ip 10.0.0.5");
	CHECK(access.ipBans().empty());
	// Whitelist
	Commands::run(f.server, *admin, "whitelist on");
	CHECK(access.whitelistEnabled());
	refusal = access.loginRefusal(mallory, "");
	CHECK(refusal && refusal->key == "multiplayer.disconnect.not_whitelisted");
	Commands::run(f.server, *admin, "whitelist add Mallory");
	CHECK(!access.loginRefusal(mallory, ""));
	Commands::run(f.server, *admin, "whitelist remove Mallory");
	CHECK(access.loginRefusal(mallory, ""));
	access.op(mallory, "Mallory");
	CHECK(!access.loginRefusal(mallory, "")); // Ops always may join
	Commands::run(f.server, *admin, "whitelist off");
	CHECK(!access.whitelistEnabled());
}

// The four files keep their vanilla format; an entry written by hand with only a name works too
TEST(server_access_files) {
	auto directory = std::filesystem::temp_directory_path() / "mc-cpp-server-access-test";
	std::filesystem::remove_all(directory);
	std::filesystem::create_directories(directory);
	{
		std::ofstream ops(directory / "ops.json");
		ops << R"([{"name": "Admin", "level": 3}])";
	}
	ServerAccess access;
	access.load(directory);
	CHECK_EQ(access.permissionLevel(UUID::fromOfflinePlayer("Admin")), 3);
	access.ban(UUID::fromOfflinePlayer("Eve"), "Eve", "Admin", "spam");
	access.banIp("192.168.1.20", "Admin", "spam");
	access.addToWhitelist(UUID::fromOfflinePlayer("Dan"), "Dan");
	ServerAccess reloaded;
	reloaded.load(directory);
	CHECK(reloaded.isBanned(UUID::fromOfflinePlayer("Eve")));
	CHECK(reloaded.isIpBanned("192.168.1.20"));
	CHECK(reloaded.isWhitelisted(UUID::fromOfflinePlayer("Dan")));
	CHECK(reloaded.bans().at(0).reason == "spam");
	CHECK(reloaded.bans().at(0).created.size() == 25); // "2026-11-24 21:00:00 +0000"
	CHECK(ServerAccess::isIpAddress("127.0.0.1") && !ServerAccess::isIpAddress("256.0.0.1") && !ServerAccess::isIpAddress("1.2.3"));
	std::filesystem::remove_all(directory);
}

// The server console: level 4, ops players; the commands that need a player (or name none) are refused
TEST(commands_from_console) {
	LevelFixture f;
	Commands::registerCommands();
	auto bob = makePlayer(f);
	bob->setUUID(UUID::fromOfflinePlayer("Bob"));
	bob->setPermissionLevel(0);
	f.addPlayer(bob);
	Player& console = f.server.console();
	CHECK(console.isConsole() && console.permissionLevel() == 4);
	Commands::run(f.server, console, "op Bob");
	CHECK_EQ(bob->permissionLevel(), 4);
	Commands::run(f.server, console, "gamemode creative"); // No target: refused
	CHECK(console.getGameMode() != GameMode::Creative);
	CHECK(bob->getGameMode() != GameMode::Creative);
	Commands::run(f.server, console, "kill @s"); // Not a player
	CHECK(!console.combat().dead);
	CHECK(Commands::englishText("commands.op.success", {"Bob"}) == "Made Bob a server operator");
	CHECK(Commands::englishText("commands.banlist.entry", {"Eve", "Server", "spam"}) == "Eve was banned by Server: spam");
	CHECK(Commands::englishText("some.key", {"a"}) == "some.key a");
}

// Text components from JSON (as /title takes them) become NBT; plain text stays plain
TEST(text_component_json) {
	Buffer plain;
	CHECK(TextComponent::writeJson(plain, "Hello there"));
	Buffer expected;
	TextComponent::writeText(expected, "Hello there");
	CHECK(plain.getData() == expected.getData());
	Buffer styled;
	CHECK(TextComponent::writeJson(styled, R"({"text":"Hi","bold":true,"extra":["a",{"text":"b"}]})"));
	const auto& bytes = styled.getData();
	CHECK(!bytes.empty() && bytes[0] == 10 && bytes.back() == 0); // A compound
	Buffer bad;
	CHECK(!TextComponent::writeJson(bad, "{not json"));
	CHECK(bad.getData().empty());
}

// /title: every form runs for the player itself (the packets aren't readable here: no crash, no wrong usage)
TEST(commands_title) {
	LevelFixture f;
	Commands::registerCommands();
	auto player = makePlayer(f);
	f.addPlayer(player);
	for (const char* line : {"title @s title Hello", "title @s subtitle {\"text\":\"sub\",\"color\":\"gold\"}", "title @s actionbar hi",
							 "title @s times 10 70 20", "title @s clear", "title @s reset", "title @s times 1 2", "title nobody clear"}) {
		Commands::run(f.server, *player, line);
	}
	CHECK(Commands::englishText("commands.title.show.title.single", {"Bob"}) == "Showing new title for Bob");
}
