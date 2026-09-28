#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/Survival.hpp"
#include "world/item/ItemUse.hpp"

#include <cmath>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1; // First air layer, on grass

	std::shared_ptr<Player> makePlayer(LevelFixture& f) {
		auto player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
		player->setPosition(0.5, Y, 0.5);
		player->setOnGround(true);
		return player;
	}
	void hold(LevelFixture& f, Player& player, const char* item, int count = 1) { player.inventory().set(player.handSlot(0), ItemStack(f.item(item), count)); }
	const ItemStack& held(Player& player) { return player.getStackInHand(0); }
	bool near(float a, float b) { return std::fabs(a - b) < 1.0E-4F; }
	// Uses the held item until it completes (or `ticks` ticks)
	void useFor(LevelFixture& f, Player& player, int ticks) {
		for (int i = 0; i < ticks; i++) ItemUse::updatingUsingItem(*f.level, player);
	}
	int countInInventory(Player& player, int item) {
		int count = 0;
		for (int slot = 9; slot < PlayerInventory::SIZE; slot++) {
			if (player.inventory().get(slot).item == item) count += player.inventory().get(slot).count;
		}
		return count;
	}
} // namespace

// FoodData.tick: exhaustion over 4 takes saturation first, then food (not in peaceful)
TEST(survival_exhaustion) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	FoodData&	 food	= player->foodData();
	food.setExhaustion(4.5F);
	Survival::tickFood(*f.level, *player);
	CHECK(near(food.getSaturationLevel(), 4.0F));
	CHECK(near(food.getExhaustionLevel(), 0.5F));
	CHECK_EQ(food.getFoodLevel(), 20);
	food.setSaturation(0);
	food.setExhaustion(4.5F);
	Survival::tickFood(*f.level, *player);
	CHECK_EQ(food.getFoodLevel(), 19);
	f.server.getConfig().setDifficulty("peaceful");
	food.setExhaustion(4.5F);
	Survival::tickFood(*f.level, *player);
	CHECK_EQ(food.getFoodLevel(), 19);
	// Exhaustion is capped at 40 and creative players get none
	food.setExhaustion(0);
	Survival::causeFoodExhaustion(*player, 100.0F);
	CHECK(near(food.getExhaustionLevel(), 40.0F));
	player->setGameMode(GameMode::Creative);
	food.setExhaustion(0);
	Survival::causeFoodExhaustion(*player, 1.0F);
	CHECK(near(food.getExhaustionLevel(), 0.0F));
}

// Full food with saturation: every 10 ticks, health from saturation; food >= 18: 1 health every 80 ticks
TEST(survival_regeneration) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	FoodData&	 food	= player->foodData();
	player->combat().health = 10;
	for (int i = 0; i < 9; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 10.0F));
	Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 10.0F + 5.0F / 6.0F));
	CHECK(near(food.getExhaustionLevel(), 5.0F));

	food.setFoodLevel(18);
	food.setSaturation(0);
	food.setExhaustion(0);
	food.setTickTimer(0);
	player->combat().health = 10;
	for (int i = 0; i < 79; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 10.0F));
	Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 11.0F));
	CHECK(near(food.getExhaustionLevel(), 6.0F));

	// Below 18: nothing, and the timer starts again
	food.setFoodLevel(17);
	food.setExhaustion(0);
	for (int i = 0; i < 200; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 11.0F));
	CHECK_EQ(food.getTickTimer(), 0);
}

// Peaceful: 1 health and 1 saturation every 20 ticks, 1 food every 10 (ServerPlayer.tickRegeneration)
TEST(survival_peaceful_regeneration) {
	LevelFixture f;
	f.server.getConfig().setDifficulty("peaceful");
	auto player = makePlayer(f);
	player->foodData().setFoodLevel(10);
	player->combat().health = 10;
	for (int i = 0; i < 20; i++) Survival::tick(*f.level, *player);
	CHECK_EQ(player->foodData().getFoodLevel(), 12);
	CHECK(player->combat().health >= 11.0F);
	// SET_HEALTH was sent with what the player has now
	CHECK(near(player->survival().lastSentHealth, player->combat().health));
	CHECK_EQ(player->survival().lastSentFood, 12);
}

// Starving: 1 damage every 80 ticks, down to 10 health in easy, 1 in normal
TEST(survival_starvation) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->foodData().setFoodLevel(0);
	player->foodData().setSaturation(0);
	player->combat().health = 2;
	for (int i = 0; i < 79; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 2.0F));
	Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 1.0F));
	player->combat().invulnerableUntil = 0;
	for (int i = 0; i < 80; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 1.0F));

	f.server.getConfig().setDifficulty("easy");
	player->combat().health = 11;
	player->combat().lastDamage = 0;
	for (int i = 0; i < 80; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 10.0F));
	player->combat().invulnerableUntil = 0;
	player->combat().lastDamage		   = 0;
	for (int i = 0; i < 80; i++) Survival::tickFood(*f.level, *player);
	CHECK(near(player->combat().health, 10.0F));
}

// Movements, jumps: sprinting costs 0.1 per block, walking nothing; a jump 0.05, a sprint jump 0.2
TEST(survival_movement_exhaustion) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	FoodData&	 food	= player->foodData();
	Survival::checkMovementStatistics(*f.level, *player, 1.0, 0, 0);
	CHECK(near(food.getExhaustionLevel(), 0.0F));
	player->setSprinting(true);
	Survival::checkMovementStatistics(*f.level, *player, 1.0, 0, 0);
	CHECK(near(food.getExhaustionLevel(), 0.1F));
	food.setExhaustion(0);
	Survival::onMove(*f.level, *player, 0, 0.4, 0, false); // Left the ground going up: a jump
	CHECK(near(food.getExhaustionLevel(), 0.2F));
	player->setSprinting(false);
	player->setOnGround(true);
	food.setExhaustion(0);
	Survival::onMove(*f.level, *player, 0, 0.4, 0, false);
	CHECK(near(food.getExhaustionLevel(), 0.05F));
	// Going up in the air isn't a jump
	food.setExhaustion(0);
	Survival::onMove(*f.level, *player, 0, 0.4, 0, false);
	CHECK(near(food.getExhaustionLevel(), 0.0F));
}

// Eating an apple takes 32 ticks, gives 4 food and 2.4 saturation, and uses one apple
TEST(survival_eat_apple) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->foodData().setFoodLevel(10);
	player->foodData().setSaturation(0);
	hold(f, *player, "minecraft:apple", 2);
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Consume);
	CHECK(player->isUsingItem());
	CHECK(player->survival().dirtyData & Survival::DATA_LIVING_FLAGS);
	useFor(f, *player, 31);
	CHECK(player->isUsingItem());
	CHECK_EQ(player->foodData().getFoodLevel(), 10);
	useFor(f, *player, 1);
	CHECK(!player->isUsingItem());
	CHECK_EQ(player->foodData().getFoodLevel(), 14);
	CHECK(near(player->foodData().getSaturationLevel(), 2.4F));
	CHECK_EQ(held(*player).count, 1);

	// Not hungry: can't eat (golden apples can: can_always_eat)
	player->foodData().setFoodLevel(20);
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Fail);
	CHECK(!player->isUsingItem());
	hold(f, *player, "minecraft:golden_apple");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Consume);
	useFor(f, *player, 32);
	CHECK(held(*player).isEmpty());

	// Changing the held item stops eating
	hold(f, *player, "minecraft:golden_apple");
	ItemUse::useItem(*f.level, *player, 0);
	hold(f, *player, "minecraft:stick");
	useFor(f, *player, 1);
	CHECK(!player->isUsingItem());
	// Released before the end: nothing eaten
	hold(f, *player, "minecraft:golden_apple");
	ItemUse::useItem(*f.level, *player, 0);
	useFor(f, *player, 10);
	ItemUse::releaseUsingItem(*f.level, *player);
	CHECK(!player->isUsingItem());
	CHECK_EQ(held(*player).count, 1);
}

// Stews leave their bowl, drinks their bottle or bucket; creative keeps the item
TEST(survival_use_remainders) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->foodData().setFoodLevel(4);
	hold(f, *player, "minecraft:mushroom_stew");
	ItemUse::useItem(*f.level, *player, 0);
	useFor(f, *player, 32);
	CHECK_EQ(held(*player).item, f.item("minecraft:bowl"));
	CHECK_EQ(player->foodData().getFoodLevel(), 10);

	hold(f, *player, "minecraft:milk_bucket");
	ItemUse::useItem(*f.level, *player, 0);
	useFor(f, *player, 32);
	CHECK_EQ(held(*player).item, f.item("minecraft:bucket"));

	hold(f, *player, "minecraft:potion");
	ItemUse::useItem(*f.level, *player, 0);
	useFor(f, *player, 32);
	CHECK_EQ(held(*player).item, f.item("minecraft:glass_bottle"));

	// Honey: drunk in 40 ticks even when full, the bottle goes to the inventory when some honey is left
	player->foodData().setFoodLevel(20);
	hold(f, *player, "minecraft:honey_bottle", 2);
	ItemUse::useItem(*f.level, *player, 0);
	useFor(f, *player, 39);
	CHECK(player->isUsingItem());
	useFor(f, *player, 1);
	CHECK_EQ(held(*player).item, f.item("minecraft:honey_bottle"));
	CHECK_EQ(held(*player).count, 1);
	CHECK_EQ(countInInventory(*player, f.item("minecraft:glass_bottle")), 1);

	player->setGameMode(GameMode::Creative);
	hold(f, *player, "minecraft:mushroom_stew");
	ItemUse::useItem(*f.level, *player, 0);
	useFor(f, *player, 32);
	CHECK_EQ(held(*player).item, f.item("minecraft:mushroom_stew"));
}

// Buckets used in the air: an empty one takes the water source looked at, a full one pours it next to the block
TEST(survival_bucket_use) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	f.set(0, LevelFixture::SURFACE, 0, "minecraft:water[level=0]");
	player->setRotation(0, 90); // Looking down
	hold(f, *player, "minecraft:bucket");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Success);
	CHECK_EQ(held(*player).item, f.item("minecraft:water_bucket"));
	CHECK(f.level->blocks().isAir(f.at(0, LevelFixture::SURFACE, 0)));

	// Nothing to take from air over grass... the ray reaches the dirt below the hole: not a fluid
	hold(f, *player, "minecraft:bucket");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Fail);
	CHECK_EQ(held(*player).item, f.item("minecraft:bucket"));

	// Poured on the side of the block hit (the top of the dirt): back where it was
	hold(f, *player, "minecraft:water_bucket");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Success);
	CHECK_EQ(held(*player).item, f.item("minecraft:bucket"));
	CHECK(f.nameAt(0, LevelFixture::SURFACE, 0) == "minecraft:water[level=0]");

	// Looking at the sky: nothing
	player->setRotation(0, -90);
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Pass);

	// Creative: the empty bucket stays, a water bucket goes to the inventory
	player->setRotation(0, 90);
	player->setGameMode(GameMode::Creative);
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Success);
	CHECK_EQ(held(*player).item, f.item("minecraft:bucket"));
	CHECK_EQ(countInInventory(*player, f.item("minecraft:water_bucket")), 1);
}

// Waterlogging: a water bucket on a slab fills it, an empty bucket takes the water back
TEST(survival_bucket_waterlog) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	player->setPosition(0.5, Y + 1, 0.5);
	f.set(0, Y, 0, "minecraft:oak_slab[type=bottom,waterlogged=false]");
	player->setRotation(0, 90);
	hold(f, *player, "minecraft:water_bucket");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Success);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:oak_slab[type=bottom,waterlogged=true]");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Success);
	CHECK(f.nameAt(0, Y, 0) == "minecraft:oak_slab[type=bottom,waterlogged=false]");
	CHECK_EQ(held(*player).item, f.item("minecraft:water_bucket"));
}

// Right click with armor wears it
TEST(survival_equip_armor) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	hold(f, *player, "minecraft:iron_helmet");
	CHECK(ItemUse::useItem(*f.level, *player, 0) == ItemUse::Result::Success);
	CHECK_EQ(player->inventory().get(5).item, f.item("minecraft:iron_helmet"));
	CHECK(held(*player).isEmpty());
	// Another one swaps them
	hold(f, *player, "minecraft:diamond_helmet");
	ItemUse::useItem(*f.level, *player, 0);
	CHECK_EQ(player->inventory().get(5).item, f.item("minecraft:diamond_helmet"));
	CHECK_EQ(held(*player).item, f.item("minecraft:iron_helmet"));
}

// Under water the air goes down by 1 per tick; at -20: 2 drowning damage and back to 0. Out of water: + 4 per tick
TEST(survival_drowning) {
	LevelFixture f;
	auto		 player = makePlayer(f);
	f.set(0, Y, 0, "minecraft:water[level=0]");
	f.set(0, Y + 1, 0, "minecraft:water[level=0]");
	Survival::tick(*f.level, *player);
	CHECK(player->survival().eyeInWater);
	CHECK_EQ(player->getAirSupply(), 299);
	CHECK(player->survival().dirtyData & Survival::DATA_AIR_SUPPLY);
	for (int i = 1; i < 319; i++) Survival::tick(*f.level, *player);
	CHECK_EQ(player->getAirSupply(), -19);
	CHECK(near(player->combat().health, 20.0F));
	Survival::tick(*f.level, *player);
	CHECK_EQ(player->getAirSupply(), 0);
	CHECK(near(player->combat().health, 18.0F));

	// Out of the water
	f.set(0, Y + 1, 0, "minecraft:air");
	f.set(0, Y, 0, "minecraft:air");
	Survival::tick(*f.level, *player);
	CHECK(!player->survival().eyeInWater);
	CHECK_EQ(player->getAirSupply(), 4);
	for (int i = 0; i < 80; i++) Survival::tick(*f.level, *player);
	CHECK_EQ(player->getAirSupply(), 300);

	// Creative players don't lose air; eyes above the surface of a single water block: not under water
	f.set(0, Y, 0, "minecraft:water[level=0]");
	Survival::tick(*f.level, *player);
	CHECK(!player->survival().eyeInWater);
	CHECK(player->survival().inWater);
	f.set(0, Y + 1, 0, "minecraft:water[level=0]");
	player->setGameMode(GameMode::Creative);
	for (int i = 0; i < 10; i++) Survival::tick(*f.level, *player);
	CHECK_EQ(player->getAirSupply(), 300);
}

// The ray cast finds the face looked at, through the outline shapes
TEST(survival_clip) {
	LevelFixture	   f;
	ItemUse::HitResult hit = ItemUse::clip(*f.level, {0.5, Y + 1.5, 0.5}, {0.5, Y - 2.0, 0.5}, false);
	CHECK(hit.hit);
	CHECK((hit.pos == BlockPos{0, LevelFixture::SURFACE, 0}));
	CHECK(hit.face == Direction::Up);
	CHECK(std::fabs(hit.location.y - Y) < 1.0E-6);
	// A bottom slab is hit at its half
	f.set(3, Y, 0, "minecraft:stone_slab[type=bottom,waterlogged=false]");
	hit = ItemUse::clip(*f.level, {0.5, Y + 0.25, 0.5}, {5.5, Y + 0.25, 0.5}, false);
	CHECK(hit.hit);
	CHECK((hit.pos == BlockPos{3, Y, 0}));
	CHECK(hit.face == Direction::West);
	hit = ItemUse::clip(*f.level, {0.5, Y + 0.75, 0.5}, {5.5, Y + 0.75, 0.5}, false);
	CHECK(!hit.hit);
}
