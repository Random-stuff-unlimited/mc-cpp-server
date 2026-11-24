#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/PlaceContext.hpp"
#include "world/blockentity/SignBlockEntity.hpp"
#include "world/blocks/Signs.hpp"

// Signs: placement (floor, wall, ceiling), editing, dyes and wax

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	std::shared_ptr<Player> makePlayer(LevelFixture& f, const char* name, double x, double z) {
		auto player = std::make_shared<Player>(name, PlayerState::Play, -1, f.server);
		player->setUUID(UUID::fromOfflinePlayer(name));
		player->setPosition(x, Y, z);
		player->setGameMode(GameMode::Survival);
		f.addPlayer(player);
		return player;
	}

	// The state a player clicking `face` would place at `at`, looking with this yaw and pitch
	int placed(LevelFixture& f, const char* item, BlockPos at, Direction face, float yaw, float pitch, bool sneaking = false) {
		PlaceContext context{};
		context.clickedPos	   = at;
		context.clickedFace	   = face;
		context.clickX		   = at.x + 0.5;
		context.clickY		   = at.y + 0.5;
		context.clickZ		   = at.z + 0.5;
		context.replaceClicked = false;
		context.yaw			   = yaw;
		context.pitch		   = pitch;
		context.secondaryUse   = sneaking;
		context.item		   = f.item(item);
		context.block		   = f.block(item);
		return f.level->behaviors().placer(context.block).getStateForPlacement(*f.level, context);
	}

	SignBlockEntity* sign(LevelFixture& f, BlockPos pos) { return dynamic_cast<SignBlockEntity*>(f.level->getBlockEntity(pos)); }

	void useWith(LevelFixture& f, Player& player, BlockPos pos, const char* item) {
		player.setSelectedSlot(0);
		player.inventory().set(player.handSlot(0), item ? ItemStack(f.item(item), 1) : ItemStack());
		int		  state	 = f.level->getBlockState(pos);
		BlockHit  hit{pos, Direction::South, pos.x + 0.5, pos.y + 0.5, pos.z + 1.0};
		UseResult result = f.level->behavior(state).useItemOn(*f.level, pos, state, player, 0, hit);
		if (result == UseResult::TryWithEmptyHand) f.level->behavior(state).useWithoutItem(*f.level, pos, state, player);
	}
} // namespace

// StandingAndWallBlockItem: on the floor it turns to the player (16 steps), against a wall it hangs on it, under a
// block a hanging sign hangs on chains when sneaking or under a block that isn't full, on a bracket otherwise
TEST(sign_placement) {
	LevelFixture f(1);
	int standing = placed(f, "minecraft:oak_sign", {0, Y, 0}, Direction::Up, 90.0F, 60.0F); // Looking west and down
	CHECK(f.data.getBlockStateName(standing).find("minecraft:oak_sign[") == 0);
	CHECK(f.data.getProperty(standing, "rotation") == "12"); // Facing east (back at the player)
	f.set(0, Y, -1, "minecraft:stone");
	int wall = placed(f, "minecraft:oak_sign", {0, Y, 0}, Direction::South, 180.0F, 0.0F); // Looking north at the stone
	CHECK(f.data.getBlockStateName(wall).find("minecraft:oak_wall_sign[") == 0);
	CHECK(f.data.getProperty(wall, "facing") == "south");
	// Nothing behind: it stands on the ground instead (the next direction looked at); floating, nowhere
	CHECK(f.data.getBlockStateName(placed(f, "minecraft:oak_sign", {3, Y, 3}, Direction::South, 180.0F, 0.0F)).find("minecraft:oak_sign[") == 0);
	CHECK_EQ(placed(f, "minecraft:oak_sign", {3, Y + 3, 3}, Direction::South, 180.0F, 0.0F), -1);
	f.set(5, Y + 2, 5, "minecraft:stone");
	int bracket = placed(f, "minecraft:oak_hanging_sign", {5, Y + 1, 5}, Direction::Down, 0.0F, -60.0F);
	CHECK(f.data.getBlockStateName(bracket).find("minecraft:oak_hanging_sign[") == 0);
	CHECK(f.data.getProperty(bracket, "attached") == "false");
	int chains = placed(f, "minecraft:oak_hanging_sign", {5, Y + 1, 5}, Direction::Down, 0.0F, -60.0F, true);
	CHECK(f.data.getProperty(chains, "attached") == "true");
	// The wall sign falls when its wall goes
	f.level->setBlock({0, Y, 0}, wall, Level::UPDATE_ALL);
	CHECK(sign(f, {0, Y, 0}) != nullptr);
	f.set(0, Y, -1, "minecraft:air");
	CHECK(f.nameAt(0, Y, 0) == "minecraft:air");
}

// Editing: a placed sign opens its placer's editor; only that player's text is taken (without formatting codes); it
// keeps its text through a save. Dyes and glow ink go on the side the player faces; honeycomb locks the sign
TEST(sign_editing_dye_and_wax) {
	LevelFixture f(1);
	auto alice = makePlayer(f, "Alice", 0.5, 3.5); // South of the sign, in front of its face
	auto eve   = makePlayer(f, "Eve", 0.5, 4.5);
	f.level->setBlock({0, Y, 0}, f.data.withProperty(f.data.getDefaultBlockState("minecraft:oak_sign"), "rotation", "0"), Level::UPDATE_ALL);
	SignBlockEntity* entity = sign(f, {0, Y, 0});
	CHECK(entity != nullptr);
	if (!entity) return;
	Signs::afterPlaced(*f.level, *alice, {0, Y, 0});
	CHECK(entity->canEdit(alice->getUUID()));
	Signs::handleUpdate(*f.level, *eve, {0, Y, 0}, true, {"hacked", "", "", ""}); // Not allowed
	CHECK(entity->text(true).messages[0].empty());
	Signs::handleUpdate(*f.level, *alice, {0, Y, 0}, true, {"Hello", "\xC2\xA7" "cworld", "", ""});
	CHECK(entity->text(true).messages[0] == "Hello");
	CHECK(entity->text(true).messages[1] == "world");
	CHECK(!entity->canEdit(alice->getUUID())); // The editor closed
	CHECK(Signs::isFacingFrontText(*f.level, {0, Y, 0}, *alice));

	useWith(f, *alice, {0, Y, 0}, "minecraft:red_dye");
	CHECK(entity->text(true).color == "red");
	CHECK(alice->inventory().get(alice->handSlot(0)).isEmpty()); // Used
	useWith(f, *alice, {0, Y, 0}, "minecraft:glow_ink_sac");
	CHECK(entity->text(true).glowingText);
	CHECK(!entity->text(false).glowingText);

	// Clicking it again opens the editor (the back side for a player behind it)
	alice->setPosition(0.5, Y, -3.5);
	useWith(f, *alice, {0, Y, 0}, nullptr);
	CHECK(entity->canEdit(alice->getUUID()));
	Signs::handleUpdate(*f.level, *alice, {0, Y, 0}, false, {"", "back", "", ""});
	CHECK(entity->text(false).messages[1] == "back");

	useWith(f, *alice, {0, Y, 0}, "minecraft:honeycomb");
	CHECK(entity->isWaxed());
	useWith(f, *alice, {0, Y, 0}, nullptr);
	CHECK(!entity->canEdit(alice->getUUID())); // Waxed: no editor

	BlockEntityWriter out(f.data);
	entity->save(out);
	SignBlockEntity	  copy("minecraft:sign", {0, Y, 0});
	BlockEntityReader in(f.data, out.out.data(), out.out.size());
	copy.load(in);
	CHECK(copy.text(true).messages[0] == "Hello" && copy.text(true).color == "red" && copy.text(false).messages[1] == "back" && copy.isWaxed());
	BlockEntityReader empty(f.data, nullptr, 0);
	SignBlockEntity	  blank("minecraft:sign", {0, Y, 0});
	blank.load(empty); // An old save without sign data
	CHECK(!blank.text(true).hasMessage());
}
