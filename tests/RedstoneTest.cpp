#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/PlaceContext.hpp"

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	std::string wire(int power) {
		return "minecraft:redstone_wire[east=side,north=none,power=" + std::to_string(power) + ",south=none,west=side]";
	}
	const char* LEVER_OFF = "minecraft:lever[face=floor,facing=north,powered=false]";
	const char* LEVER_ON  = "minecraft:lever[face=floor,facing=north,powered=true]";

	// Right-clicks the block at pos, like a player
	void use(LevelFixture& f, Player& player, int x, int y, int z) {
		int state = f.at(x, y, z);
		f.level->behavior(state).useWithoutItem(*f.level, {x, y, z}, state, player);
	}
	int power(LevelFixture& f, int x, int y, int z) {
		int state = f.at(x, y, z);
		return f.data.getBlocks().getInt(state, f.data.getBlocks().property("power"));
	}
	bool lampLit(LevelFixture& f, int x, int y, int z) { return f.nameAt(x, y, z) == "minecraft:redstone_lamp[lit=true]"; }
} // namespace

// A lever powers a line of wire: one less per block, down to 0 after 15 blocks; the lamp at the end stays off
TEST(redstone_wire_line) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setGameMode(GameMode::Creative);
	f.set(0, Y, 0, LEVER_OFF);
	for (int x = 1; x <= 16; x++) f.set(x, Y, 0, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	f.set(3, Y, 1, "minecraft:redstone_lamp[lit=false]"); // Next to the wire's side: wires only power what they point at
	f.set(17, Y, 0, "minecraft:redstone_lamp[lit=false]");
	f.tick(1);
	CHECK(f.nameAt(5, Y, 0) == wire(0)); // Wires connected into a line
	use(f, *player, 0, Y, 0);
	f.tick(1);
	CHECK(f.nameAt(0, Y, 0) == LEVER_ON);
	for (int x = 1; x <= 16; x++) CHECK_EQ(power(f, x, Y, 0), 16 - x);
	CHECK(!lampLit(f, 3, Y, 1));
	CHECK(!lampLit(f, 17, Y, 0)); // Power 0 at the last wire
	f.set(16, Y, 0, "minecraft:redstone_lamp[lit=false]");
	CHECK(lampLit(f, 16, Y, 0));
	// Off again: the whole line goes back to 0 at once, the lamp 4 ticks later
	use(f, *player, 0, Y, 0);
	for (int x = 1; x <= 15; x++) CHECK_EQ(power(f, x, Y, 0), 0);
	CHECK(lampLit(f, 16, Y, 0));
	f.tick(4);
	CHECK(!lampLit(f, 16, Y, 0));
}

// A torch on a powered block turns off 2 ticks later
TEST(redstone_torch_inverter) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	f.set(0, Y, 0, "minecraft:stone");
	f.set(0, Y + 1, 0, "minecraft:redstone_torch[lit=true]");
	f.set(1, Y, 0, "minecraft:lever[face=wall,facing=east,powered=false]");
	f.tick(1);
	use(f, *player, 1, Y, 0);
	f.tick(1);
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:redstone_torch[lit=true]");
	f.tick(1);
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:redstone_torch[lit=false]");
	use(f, *player, 1, Y, 0);
	f.tick(2);
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:redstone_torch[lit=true]");
}

// A repeater with delay 4 waits 8 ticks, a comparator 2
TEST(redstone_repeater_and_comparator_delays) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	// Lever -> repeater (facing west: input from the west... its FACING points to the input) -> lamp
	f.set(0, Y, 0, LEVER_OFF);
	f.set(1, Y, 0, "minecraft:repeater[delay=4,facing=west,locked=false,powered=false]");
	f.set(2, Y, 0, "minecraft:redstone_lamp[lit=false]");
	f.set(0, Y, 4, LEVER_OFF);
	f.set(1, Y, 4, "minecraft:comparator[facing=west,mode=compare,powered=false]");
	f.set(2, Y, 4, "minecraft:redstone_lamp[lit=false]");
	f.tick(1);
	use(f, *player, 0, Y, 0);
	use(f, *player, 0, Y, 4);
	f.tick(1);
	CHECK(!lampLit(f, 2, Y, 4));
	f.tick(1);
	CHECK(lampLit(f, 2, Y, 4));
	f.tick(5); // 7 ticks after the lever
	CHECK(!lampLit(f, 2, Y, 0));
	f.tick(1);
	CHECK(lampLit(f, 2, Y, 0));
	CHECK_EQ(f.level->comparatorOutput({1, Y, 4}), 15);
}

// Comparator in subtract mode: back minus side
TEST(redstone_comparator_subtract) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->setGameMode(GameMode::Creative);
	f.set(0, Y, 0, "minecraft:redstone_block");
	f.set(1, Y, 0, "minecraft:comparator[facing=west,mode=compare,powered=false]");
	f.set(3, Y, 0, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	f.set(1, Y, 1, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	f.set(1, Y, 2, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	f.set(1, Y, 3, "minecraft:redstone_block"); // Side input: the wire next to it at 15, then 14
	f.tick(4);
	CHECK_EQ(power(f, 1, Y, 1), 14);
	CHECK_EQ(f.level->comparatorOutput({1, Y, 0}), 15); // Compare: 15 >= 14
	use(f, *player, 1, Y, 0);
	f.tick(4);
	CHECK_EQ(f.level->comparatorOutput({1, Y, 0}), 1);
	f.set(2, Y, 0, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	f.tick(4);
	CHECK_EQ(power(f, 2, Y, 0), 1);
}

// An observer gives a 2-tick pulse when the block it faces changes
TEST(redstone_observer_pulse) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:observer[facing=west,powered=false]"); // Watches x = -1, outputs to x = 1
	f.set(1, Y, 0, "minecraft:redstone_lamp[lit=false]");
	f.tick(5);
	f.set(-1, Y, 0, "minecraft:stone");
	f.tick(1);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:observer[facing=west,powered=false]");
	f.tick(1);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:observer[facing=west,powered=true]");
	CHECK(lampLit(f, 1, Y, 0));
	f.tick(2);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:observer[facing=west,powered=false]");
	f.tick(4);
	CHECK(!lampLit(f, 1, Y, 0));
}

// A torch powering its own block burns out after 8 toggles within 60 ticks
TEST(redstone_torch_burnout) {
	LevelFixture f;
	// Torch on the side of a block, wire from the torch back onto the block: a 1-torch clock
	f.set(0, Y, 0, "minecraft:stone");
	f.set(1, Y, 0, "minecraft:redstone_wall_torch[facing=east,lit=true]");
	f.set(1, Y - 1, 0, "minecraft:stone");
	f.set(0, Y + 1, 0, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	f.set(1, Y + 1, 0, "minecraft:stone");
	f.set(1, Y + 2, 0, "minecraft:redstone_wire[east=none,north=none,power=0,south=none,west=none]");
	int toggles = 0;
	std::string last = f.nameAt(1, Y, 0);
	for (int i = 0; i < 100; i++) {
		f.tick(1);
		if (f.nameAt(1, Y, 0) != last) toggles++;
		last = f.nameAt(1, Y, 0);
	}
	CHECK(toggles > 0 && toggles < 20); // Burnt out after a few toggles instead of 50
}

// Buttons stay pressed 20 ticks (stone) or 30 ticks (wood)
TEST(redstone_buttons) {
	LevelFixture f;
	auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	f.set(0, Y, 0, "minecraft:stone_button[face=floor,facing=north,powered=false]");
	f.set(2, Y, 0, "minecraft:oak_button[face=floor,facing=north,powered=false]");
	f.tick(1);
	use(f, *player, 0, Y, 0);
	use(f, *player, 2, Y, 0);
	f.tick(19);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:stone_button[face=floor,facing=north,powered=true]");
	f.tick(1);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:stone_button[face=floor,facing=north,powered=false]");
	f.tick(9);
	CHECK(f.nameAt(2, Y, 0) == "minecraft:oak_button[face=floor,facing=north,powered=true]");
	f.tick(1);
	CHECK(f.nameAt(2, Y, 0) == "minecraft:oak_button[face=floor,facing=north,powered=false]");
}

// Placement: repeaters face away from the player, levers take the clicked face
TEST(redstone_placement) {
	LevelFixture f;
	PlaceContext context{};
	context.clickedPos	   = {0, Y, 0};
	context.clickedFace	   = Direction::Up;
	context.replaceClicked = false;
	context.yaw			   = 0.0F; // Looking south
	context.pitch		   = 60.0F;
	context.block		   = f.block("minecraft:repeater");
	int repeater		   = f.level->behavior(f.data.getDefaultBlockState("minecraft:repeater")).getStateForPlacement(*f.level, context);
	CHECK(f.data.getBlockStateName(repeater) == "minecraft:repeater[delay=1,facing=north,locked=false,powered=false]");
	f.set(1, Y, 0, "minecraft:stone");
	context.clickedFace = Direction::West; // Clicking the west face of the stone at x = 1
	context.block		= f.block("minecraft:lever");
	int lever			= f.level->behavior(f.data.getDefaultBlockState("minecraft:lever")).getStateForPlacement(*f.level, context);
	CHECK(f.data.getBlockStateName(lever) == "minecraft:lever[face=wall,facing=west,powered=false]");
}

// Directional blocks face the player: droppers and dispensers in 3D, furnaces horizontally, hoppers into the block
TEST(redstone_facing_placement) {
	LevelFixture f;
	auto place = [&](const char* block, float yaw, float pitch, Direction face) {
		PlaceContext context{};
		context.clickedPos	   = {0, Y, 0};
		context.clickedFace	   = face;
		context.replaceClicked = false;
		context.yaw			   = yaw;
		context.pitch		   = pitch;
		context.block		   = f.block(block);
		int state			   = f.level->behaviors().placer(context.block).getStateForPlacement(*f.level, context);
		return state >= 0 ? f.data.getBlockStateName(state) : std::string("none");
	};
	CHECK(place("minecraft:dropper", 0.0F, 80.0F, Direction::Up) == "minecraft:dropper[facing=up,triggered=false]");
	CHECK(place("minecraft:dropper", 0.0F, 0.0F, Direction::North) == "minecraft:dropper[facing=north,triggered=false]");
	CHECK(place("minecraft:dropper", 90.0F, 0.0F, Direction::East) == "minecraft:dropper[facing=east,triggered=false]");
	CHECK(place("minecraft:dispenser", 0.0F, -80.0F, Direction::Down) == "minecraft:dispenser[facing=down,triggered=false]");
	CHECK(place("minecraft:furnace", 90.0F, 70.0F, Direction::Up) == "minecraft:furnace[facing=east,lit=false]");
	CHECK(place("minecraft:hopper", 0.0F, 0.0F, Direction::West) == "minecraft:hopper[enabled=true,facing=east]");
	CHECK(place("minecraft:hopper", 0.0F, 0.0F, Direction::Up) == "minecraft:hopper[enabled=true,facing=down]");
}
