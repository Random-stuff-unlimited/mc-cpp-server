#include "LevelFixture.hpp"
#include "Test.hpp"
#include "network/PacketIds.hpp"
#include "player.hpp"
#include "world/PlaceContext.hpp"
#include "world/blocks/Containers.hpp"
#include "world/inventory/Menu.hpp"
#include "world/item/Components.hpp"

#include <utility>
#include <vector>

// Chests, barrels, shulker boxes, ender chests and hoppers, checked against vanilla's behavior

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	std::shared_ptr<Player> joinedPlayer(LevelFixture& f, double x, double z, const char* name = "Bob") {
		auto player = std::make_shared<Player>(name, PlayerState::Play, -1, f.server);
		player->setPosition(x, Y, z);
		player->setCompressionThreshold(f.server.getConfig().getCompressionThreshold());
		f.server.addGamePlayer(player);
		return player;
	}

	// The packets queued for the player since the last call: id and payload (compressed ones are skipped)
	std::vector<std::pair<int, std::vector<uint8_t>>> packets(LevelFixture& f, Player& player) {
		std::vector<std::pair<int, std::vector<uint8_t>>> out;
		std::vector<uint8_t>							  data;
		{
			std::lock_guard<std::mutex> lock(player.output().mutex);
			data.swap(player.output().data);
		}
		size_t pos		  = 0;
		auto   varint	  = [&](size_t& at) {
			  uint32_t value = 0;
			  for (int shift = 0; at < data.size() && shift < 35; shift += 7) {
				  uint8_t byte = data[at++];
				  value |= static_cast<uint32_t>(byte & 0x7F) << shift;
				  if (!(byte & 0x80)) break;
			  }
			  return static_cast<int>(value);
		};
		bool compressed = f.server.getConfig().getCompressionThreshold() >= 0;
		while (pos < data.size()) {
			int	   length = varint(pos);
			size_t end	  = pos + static_cast<size_t>(length);
			if (compressed && varint(pos) != 0) {
				pos = end;
				continue;
			}
			int id = varint(pos);
			out.emplace_back(id, std::vector<uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(pos), data.begin() + static_cast<std::ptrdiff_t>(end)));
			pos = end;
		}
		return out;
	}
	int countPackets(const std::vector<std::pair<int, std::vector<uint8_t>>>& list, int id) {
		int count = 0;
		for (const auto& packet : list) count += packet.first == id;
		return count;
	}
	// The data of the block events sent (type 1: the open count)
	std::vector<int> blockEventData(const std::vector<std::pair<int, std::vector<uint8_t>>>& list) {
		std::vector<int> found;
		for (const auto& [id, payload] : list) {
			if (id == PacketId::Play::Clientbound::BLOCK_EVENT && payload.size() > 9) found.push_back(payload[9]);
		}
		return found;
	}

	PlaceContext placing(LevelFixture& f, const std::string& item, BlockPos at, Direction face, float yaw, bool sneaking) {
		PlaceContext context{};
		context.clickedPos	   = at;
		context.clickedFace	   = face;
		context.clickX		   = at.x + 0.5;
		context.clickY		   = at.y + 0.5;
		context.clickZ		   = at.z + 0.5;
		context.replaceClicked = false;
		context.yaw			   = yaw;
		context.pitch		   = 0.0F;
		context.secondaryUse   = sneaking;
		context.item		   = f.item(item);
		context.block		   = f.block(item);
		return context;
	}
	// Places the block like BlockItem.place (its state for placement, set with UPDATE_ALL_IMMEDIATE)
	void place(LevelFixture& f, const std::string& item, BlockPos at, Direction face, float yaw, bool sneaking = false) {
		PlaceContext context = placing(f, item, at, face, yaw, sneaking);
		int			 state	 = f.level->behaviors().placer(context.block).getStateForPlacement(*f.level, context);
		f.level->setBlock(at, state, Level::UPDATE_ALL_IMMEDIATE);
	}
	void use(LevelFixture& f, Player& player, BlockPos pos) {
		int state = f.level->getBlockState(pos);
		f.level->behavior(state).useWithoutItem(*f.level, pos, state, player);
	}
	int comparator(LevelFixture& f, BlockPos pos) {
		int state = f.level->getBlockState(pos);
		return f.level->behavior(state).getAnalogOutputSignal(*f.level, pos, state, Direction::North);
	}
	int itemsNamed(LevelFixture& f, const char* name) {
		int count = 0;
		for (ItemEntity* item : f.items()) {
			if (item->item().item == f.item(name)) count += item->item().count;
		}
		return count;
	}
} // namespace

// ChestBlock.getStateForPlacement: a chest beside a lone one facing the same way joins it; sneaking keeps it single;
// sneaking against the side of a chest joins that one, facing its way
TEST(chest_double_joining) {
	LevelFixture f;
	// Yaw 0: the player looks south, chests face north
	place(f, "minecraft:chest", {0, Y, 0}, Direction::Up, 0.0F);
	place(f, "minecraft:chest", {1, Y, 0}, Direction::Up, 0.0F);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:chest[facing=north,type=left,waterlogged=false]");
	CHECK(f.nameAt(1, Y, 0) == "minecraft:chest[facing=north,type=right,waterlogged=false]");
	// A third one stays single
	place(f, "minecraft:chest", {2, Y, 0}, Direction::Up, 0.0F);
	CHECK(f.nameAt(2, Y, 0) == "minecraft:chest[facing=north,type=single,waterlogged=false]");
	// Sneaking: single
	place(f, "minecraft:chest", {0, Y, 3}, Direction::Up, 0.0F);
	place(f, "minecraft:chest", {1, Y, 3}, Direction::Up, 0.0F, true);
	CHECK(f.nameAt(1, Y, 3) == "minecraft:chest[facing=north,type=single,waterlogged=false]");
	CHECK(f.nameAt(0, Y, 3) == "minecraft:chest[facing=north,type=single,waterlogged=false]");
	// Sneaking against the east side of the chest at (1, 3), looking west: faces north like it, as its left half
	place(f, "minecraft:chest", {2, Y, 3}, Direction::East, 90.0F, true);
	CHECK(f.nameAt(2, Y, 3) == "minecraft:chest[facing=north,type=right,waterlogged=false]");
	CHECK(f.nameAt(1, Y, 3) == "minecraft:chest[facing=north,type=left,waterlogged=false]");
	// A trapped chest doesn't join a chest
	place(f, "minecraft:trapped_chest", {-1, Y, 0}, Direction::Up, 0.0F);
	CHECK(f.nameAt(-1, Y, 0) == "minecraft:trapped_chest[facing=north,type=single,waterlogged=false]");
	// Breaking a half makes the other single
	f.set(1, Y, 0, "minecraft:air");
	CHECK(f.nameAt(0, Y, 0) == "minecraft:chest[facing=north,type=single,waterlogged=false]");
}

// A double chest opens as 54 slots (the right half first), not when a half is blocked; comparators read both halves
TEST(chest_double_menu_and_blocked) {
	LevelFixture f;
	auto player = joinedPlayer(f, 0.5, 2.5);
	place(f, "minecraft:chest", {0, Y, 0}, Direction::Up, 0.0F);
	place(f, "minecraft:chest", {1, Y, 0}, Direction::Up, 0.0F);
	auto* left	= f.level->getBlockEntity<ChestBlockEntity>({0, Y, 0});
	auto* right = f.level->getBlockEntity<ChestBlockEntity>({1, Y, 0});
	CHECK(left && right);
	if (!left || !right) return;
	right->setItem(0, ItemStack(f.item("minecraft:stone"), 64));
	left->setItem(26, ItemStack(f.item("minecraft:dirt"), 64));
	use(f, *player, {0, Y, 0});
	Menu& menu = Menus::current(*player, *f.level);
	CHECK(menu.type() == "minecraft:generic_9x6");
	CHECK_EQ(int(menu.slots().size()), 54 + 36);
	CHECK_EQ(menu.slots()[0].item().item, f.item("minecraft:stone"));
	CHECK_EQ(menu.slots()[53].item().item, f.item("minecraft:dirt"));
	// Both halves count the opener
	CHECK_EQ(left->openCount(), 1);
	CHECK_EQ(right->openCount(), 1);
	// 2 full stacks of 54 slots: floor(2/54 * 14) + 1 = 1
	CHECK_EQ(comparator(f, {0, Y, 0}), 1);
	Menus::doCloseContainer(*player, *f.level);
	CHECK_EQ(left->openCount(), 0);

	// A solid block above the left half: neither half opens, and comparators read nothing
	f.set(0, Y + 1, 0, "minecraft:stone");
	use(f, *player, {1, Y, 0});
	CHECK(!player->openMenuSlot());
	use(f, *player, {0, Y, 0});
	CHECK(!player->openMenuSlot());
	CHECK_EQ(comparator(f, {1, Y, 0}), 0);
	// Glass isn't a redstone conductor: it opens
	f.set(0, Y + 1, 0, "minecraft:glass");
	use(f, *player, {1, Y, 0});
	CHECK(player->openMenuSlot() != nullptr);
}

// Opening a chest: lid block event with the count, one open sound, closing sound when the last one leaves; a player
// that is gone without closing is found by the recheck every 5 ticks
TEST(chest_openers_counter) {
	LevelFixture f;
	auto bob   = joinedPlayer(f, 0.5, 2.5, "Bob");
	auto alice = joinedPlayer(f, 1.5, 2.5, "Alice");
	f.set(0, Y, 0, "minecraft:chest[facing=south,type=single,waterlogged=false]");
	auto* chest = f.level->getBlockEntity<ChestBlockEntity>({0, Y, 0});
	packets(f, *bob);
	use(f, *bob, {0, Y, 0});
	use(f, *alice, {0, Y, 0});
	CHECK_EQ(chest->openCount(), 2);
	f.tick(1);
	auto sent = packets(f, *bob);
	CHECK_EQ(countPackets(sent, PacketId::Play::Clientbound::SOUND), 1); // Only the first opener
	std::vector<int> events = blockEventData(sent);
	CHECK(events == std::vector<int>({1, 2}));
	// Still open after the rechecks
	f.tick(12);
	CHECK_EQ(chest->openCount(), 2);
	// Alice's menu goes without closing (like a disconnection): the recheck finds only Bob
	alice->openMenuSlot().reset();
	f.tick(6);
	CHECK_EQ(chest->openCount(), 1);
	packets(f, *bob);
	Menus::doCloseContainer(*bob, *f.level);
	f.tick(1);
	sent = packets(f, *bob);
	CHECK_EQ(countPackets(sent, PacketId::Play::Clientbound::SOUND), 1); // The close sound
	CHECK(blockEventData(sent) == std::vector<int>({0}));
	CHECK_EQ(chest->openCount(), 0);
}

// TrappedChestBlock: its power is the number of players looking inside, strong downward
TEST(chest_trapped_signal) {
	LevelFixture f;
	auto player = joinedPlayer(f, 0.5, 2.5);
	f.set(0, Y + 1, 0, "minecraft:trapped_chest[facing=south,type=single,waterlogged=false]");
	f.set(1, Y, 0, "minecraft:stone");
	f.set(1, Y + 1, 0, "minecraft:redstone_wire[east=side,north=none,power=0,south=none,west=side]");
	use(f, *player, {0, Y + 1, 0});
	int state = f.at(0, Y + 1, 0);
	CHECK_EQ(f.level->behavior(state).getSignal(*f.level, {0, Y + 1, 0}, state, Direction::West), 1);
	CHECK_EQ(f.level->behavior(state).getDirectSignal(*f.level, {0, Y + 1, 0}, state, Direction::Up), 1);
	CHECK_EQ(f.level->behavior(state).getDirectSignal(*f.level, {0, Y + 1, 0}, state, Direction::West), 0);
	CHECK(f.nameAt(1, Y + 1, 0).find("power=1") != std::string::npos);
	Menus::doCloseContainer(*player, *f.level);
	CHECK(f.nameAt(1, Y + 1, 0).find("power=0") != std::string::npos);
}

// Copper chests: a half keeps its block entity (its items) when it oxidizes, the other half follows; they only
// oxidize closed
TEST(chest_copper_keeps_contents) {
	LevelFixture f;
	place(f, "minecraft:copper_chest", {0, Y, 0}, Direction::Up, 0.0F);
	place(f, "minecraft:exposed_copper_chest", {1, Y, 0}, Direction::Up, 0.0F);
	// Joined with the least oxidized: both plain copper
	CHECK(f.nameAt(1, Y, 0) == "minecraft:copper_chest[facing=north,type=right,waterlogged=false]");
	auto* chest = f.level->getBlockEntity<ChestBlockEntity>({0, Y, 0});
	chest->setItem(3, ItemStack(f.item("minecraft:diamond"), 5));
	int left = f.at(0, Y, 0);
	f.level->setBlock({0, Y, 0}, f.level->blocks().withPropertiesOf(f.block("minecraft:weathered_copper_chest"), left), Level::UPDATE_ALL);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:weathered_copper_chest[facing=north,type=left,waterlogged=false]");
	CHECK(f.nameAt(1, Y, 0) == "minecraft:weathered_copper_chest[facing=north,type=right,waterlogged=false]");
	CHECK(f.level->getBlockEntity<ChestBlockEntity>({0, Y, 0}) == chest);
	CHECK_EQ(chest->item(3).count, 5);
	CHECK_EQ(itemsNamed(f, "minecraft:diamond"), 0);
	// A waxed chest beside an unwaxed exposed one: unwaxed, the least oxidized
	place(f, "minecraft:exposed_copper_chest", {0, Y, 3}, Direction::Up, 0.0F);
	place(f, "minecraft:waxed_copper_chest", {1, Y, 3}, Direction::Up, 0.0F);
	CHECK(f.nameAt(1, Y, 3) == "minecraft:copper_chest[facing=north,type=right,waterlogged=false]");
	CHECK(f.nameAt(0, Y, 3) == "minecraft:copper_chest[facing=north,type=left,waterlogged=false]");
}

// BarrelBlock: "open" while a player looks inside
TEST(barrel_open_state) {
	LevelFixture f;
	auto player = joinedPlayer(f, 0.5, 2.5);
	f.set(0, Y, 0, "minecraft:barrel[facing=up,open=false]");
	use(f, *player, {0, Y, 0});
	CHECK(Menus::current(*player, *f.level).type() == "minecraft:generic_9x3");
	CHECK(f.nameAt(0, Y, 0) == "minecraft:barrel[facing=up,open=true]");
	f.tick(11);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:barrel[facing=up,open=true]");
	Menus::doCloseContainer(*player, *f.level);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:barrel[facing=up,open=false]");
}

// ShulkerBoxBlockEntity: the lid opens over 10 ticks once the block event ran; nothing opens with a block in the
// way of the lid; no shulker box inside
TEST(shulker_box_lid_and_rules) {
	LevelFixture f;
	auto player = joinedPlayer(f, 0.5, 2.5);
	f.set(0, Y, 0, "minecraft:shulker_box[facing=up]");
	auto* box = f.level->getBlockEntity<ShulkerBoxBlockEntity>({0, Y, 0});
	CHECK(box != nullptr);
	if (!box) return;
	use(f, *player, {0, Y, 0});
	CHECK(Menus::current(*player, *f.level).type() == "minecraft:shulker_box");
	CHECK(box->animation() == ShulkerBoxBlockEntity::Animation::Closed); // Until the block event
	f.tick(1);
	CHECK(box->animation() == ShulkerBoxBlockEntity::Animation::Opening);
	f.tick(10);
	CHECK(box->animation() == ShulkerBoxBlockEntity::Animation::Opened);
	// A shulker box can't go in, by the menu or by a hopper
	ItemStack other(f.item("minecraft:red_shulker_box"), 1);
	Menu&	  menu = Menus::current(*player, *f.level);
	CHECK(!menu.slots()[0].mayPlace(other, f.data));
	CHECK(menu.slots()[0].mayPlace(ItemStack(f.item("minecraft:stone"), 1), f.data));
	Direction down = Direction::Down;
	CHECK(!box->canPlaceItemThroughFace(0, other, &down));
	Menus::doCloseContainer(*player, *f.level);
	f.tick(1);
	CHECK(box->animation() == ShulkerBoxBlockEntity::Animation::Closing);
	f.tick(10);
	CHECK(box->animation() == ShulkerBoxBlockEntity::Animation::Closed);

	// Stone above a box facing up: no room for the lid
	f.set(0, Y + 1, 0, "minecraft:stone");
	use(f, *player, {0, Y, 0});
	CHECK(!player->openMenuSlot());
	// Facing north, the stone above doesn't matter
	f.set(2, Y, 0, "minecraft:shulker_box[facing=north]");
	f.set(2, Y + 1, 0, "minecraft:stone");
	use(f, *player, {2, Y, 0});
	CHECK(player->openMenuSlot() != nullptr);
}

// A broken shulker box drops itself with its items and name (loot table copy_components); placing it back gives them
// to the new box (BlockEntity.applyComponentsFromItemStack). In creative, playerWillDestroy drops it
TEST(shulker_box_keeps_contents) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:blue_shulker_box[facing=up]");
	auto* box = f.level->getBlockEntity<ShulkerBoxBlockEntity>({0, Y, 0});
	box->setItem(2, ItemStack(f.item("minecraft:diamond"), 7));
	box->customName = {8, 0, 3, 'B', 'o', 'x'}; // A string tag
	f.level->destroyBlock({0, Y, 0}, true);
	std::vector<ItemEntity*> dropped = f.items();
	CHECK_EQ(int(dropped.size()), 1);
	if (dropped.size() != 1) return;
	ItemStack stack = dropped[0]->item();
	CHECK_EQ(stack.item, f.item("minecraft:blue_shulker_box"));
	std::optional<std::vector<uint8_t>> contents = Components::get(stack, f.data, "minecraft:container");
	CHECK(contents.has_value());
	if (contents) {
		std::optional<std::vector<ItemStack>> items = Components::decodeContainer(*contents, f.data);
		CHECK(items && items->size() == 3 && (*items)[2].count == 7);
	}
	CHECK(Components::get(stack, f.data, "minecraft:custom_name").has_value());
	CHECK_EQ(itemsNamed(f, "minecraft:diamond"), 0); // Not spilled

	// Placed again
	f.set(3, Y, 0, "minecraft:blue_shulker_box[facing=up]");
	auto* placed = f.level->getBlockEntity<ShulkerBoxBlockEntity>({3, Y, 0});
	ContainerItems::apply(*placed, stack, f.data);
	CHECK_EQ(placed->item(2).count, 7);
	CHECK(placed->customName == box->customName);

	// Creative: no loot, but playerWillDestroy drops the box when it holds something
	auto player = joinedPlayer(f, 3.5, 2.5);
	player->setGameMode(GameMode::Creative);
	dropped[0]->discard();
	int state = f.at(3, Y, 0);
	f.level->behavior(state).playerWillDestroy(*f.level, {3, Y, 0}, state, *player);
	CHECK_EQ(itemsNamed(f, "minecraft:blue_shulker_box"), 1);
}

// Ender chests show the player's own items; the lid counts the openers
TEST(ender_chest_own_items) {
	LevelFixture f;
	auto bob   = joinedPlayer(f, 0.5, 2.5, "Bob");
	auto alice = joinedPlayer(f, 1.5, 2.5, "Alice");
	f.set(0, Y, 0, "minecraft:ender_chest[facing=south,waterlogged=false]");
	bob->enderChest()[0]   = ItemStack(f.item("minecraft:stone"), 3);
	alice->enderChest()[0] = ItemStack(f.item("minecraft:dirt"), 4);
	use(f, *bob, {0, Y, 0});
	use(f, *alice, {0, Y, 0});
	CHECK_EQ(Menus::current(*bob, *f.level).slots()[0].item().item, f.item("minecraft:stone"));
	CHECK_EQ(Menus::current(*alice, *f.level).slots()[0].item().count, 4);
	f.tick(1);
	CHECK(blockEventData(packets(f, *bob)) == std::vector<int>({1, 2}));
	// The recheck every 5 ticks sends the count again
	f.tick(10);
	CHECK(blockEventData(packets(f, *bob)) == std::vector<int>({2, 2}));
	Menus::doCloseContainer(*alice, *f.level);
	f.tick(1);
	CHECK(blockEventData(packets(f, *bob)) == std::vector<int>({1}));
	// Blocked above: nothing opens
	Menus::doCloseContainer(*bob, *f.level);
	f.set(0, Y + 1, 0, "minecraft:stone");
	use(f, *bob, {0, Y, 0});
	CHECK(!bob->openMenuSlot());
}

// HopperBlockEntity: one item every 8 ticks into what it faces (a double chest as one), one pulled from above
TEST(hopper_push_pull_cooldown) {
	LevelFixture f;
	// Chest (double) <- hopper <- chest above
	f.set(0, Y, 0, "minecraft:chest[facing=north,type=left,waterlogged=false]");
	f.set(1, Y, 0, "minecraft:chest[facing=north,type=right,waterlogged=false]");
	f.set(0, Y + 1, 0, "minecraft:hopper[enabled=true,facing=down]");
	f.set(0, Y + 2, 0, "minecraft:chest[facing=north,type=single,waterlogged=false]");
	auto* source = f.level->getBlockEntity<ChestBlockEntity>({0, Y + 2, 0});
	auto* hopper = f.level->getBlockEntity<HopperBlockEntity>({0, Y + 1, 0});
	auto* left	 = f.level->getBlockEntity<ChestBlockEntity>({0, Y, 0});
	auto* right	 = f.level->getBlockEntity<ChestBlockEntity>({1, Y, 0});
	source->setItem(0, ItemStack(f.item("minecraft:stone"), 3));
	// The right half (first) is full: the stone goes in the left half's first slot
	for (int i = 0; i < 27; i++) right->setItem(i, ItemStack(f.item("minecraft:dirt"), 64));
	f.tick(1);
	CHECK_EQ(hopper->item(0).count, 1); // Pulled on its first tick
	CHECK_EQ(source->item(0).count, 2);
	f.tick(7);
	CHECK_EQ(hopper->item(0).count, 1); // Cooldown
	f.tick(1);
	// Pushed first (the hopper is empty again), then pulled
	CHECK_EQ(left->item(0).count, 1);
	CHECK_EQ(hopper->item(0).count, 1);
	CHECK_EQ(source->item(0).count, 1);
	f.tick(16);
	CHECK_EQ(left->item(0).count, 3);
	CHECK(source->item(0).isEmpty());
	CHECK(hopper->isEmpty());

	// Powered: disabled
	source->setItem(0, ItemStack(f.item("minecraft:stone"), 3));
	f.set(-1, Y + 1, 0, "minecraft:redstone_block");
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:hopper[enabled=false,facing=down]");
	f.tick(20);
	CHECK_EQ(source->item(0).count, 3);
	f.set(-1, Y + 1, 0, "minecraft:air");
	CHECK(f.nameAt(0, Y + 1, 0) == "minecraft:hopper[enabled=true,facing=down]");
	f.tick(1);
	CHECK_EQ(source->item(0).count, 2);
}

// A hopper feeding another: the second one waits a full cooldown (one tick less if it ticked first)
TEST(hopper_chain_cooldown) {
	LevelFixture f;
	f.set(0, Y + 1, 0, "minecraft:hopper[enabled=true,facing=east]");
	f.set(1, Y + 1, 0, "minecraft:hopper[enabled=true,facing=east]");
	auto* first	 = f.level->getBlockEntity<HopperBlockEntity>({0, Y + 1, 0});
	auto* second = f.level->getBlockEntity<HopperBlockEntity>({1, Y + 1, 0});
	first->setItem(0, ItemStack(f.item("minecraft:stone"), 2));
	f.tick(1);
	CHECK_EQ(second->item(0).count, 1);
	// The second one ticked after the first in the same tick: cooldown 8, one tick of it spent... as vanilla: 8 - 0
	CHECK(second->cooldown == 8 || second->cooldown == 7);
}

// Worldly containers through a hopper: a shulker box takes items from any side, never a shulker box; hoppers suck
// item entities from above; comparators read a hopper
TEST(hopper_worldly_and_items) {
	LevelFixture f;
	f.set(0, Y, 0, "minecraft:shulker_box[facing=up]");
	f.set(0, Y + 1, 0, "minecraft:hopper[enabled=true,facing=down]");
	auto* hopper = f.level->getBlockEntity<HopperBlockEntity>({0, Y + 1, 0});
	auto* box	 = f.level->getBlockEntity<ShulkerBoxBlockEntity>({0, Y, 0});
	hopper->setItem(0, ItemStack(f.item("minecraft:red_shulker_box"), 1));
	hopper->setItem(1, ItemStack(f.item("minecraft:stone"), 1));
	f.tick(1);
	CHECK(box->item(0).item == f.item("minecraft:stone"));
	CHECK_EQ(hopper->item(0).count, 1);
	CHECK_EQ(comparator(f, {0, Y + 1, 0}), 3); // A full slot (shulker boxes stack to 1) of 5: floor(0.2 * 14) + 1

	// An item entity on top of it
	auto item = ItemEntity::create(*f.level, {0.5, Y + 2.0, 0.5}, ItemStack(f.item("minecraft:apple"), 3));
	item->setDeltaMovement({0.0, 0.0, 0.0});
	f.level->entities().add(std::move(item));
	f.tick(9);
	CHECK_EQ(hopper->item(1).item, f.item("minecraft:apple"));
	CHECK_EQ(hopper->item(1).count, 3);
	CHECK_EQ(itemsNamed(f, "minecraft:apple"), 0);
}
