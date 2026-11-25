#include "world/BlockBehavior.hpp"
#include "world/item/ItemUse.hpp"
#include "world/item/ItemDamage.hpp"
#include "world/entity/mobs/FlyingAnimals.hpp"
#include "world/blockentity/BeehiveBlockEntity.hpp"
#include "world/blocks/Composter.hpp"

#include "data/GameData.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/blockentity/BlockEntity.hpp"
#include "world/entity/ItemEntity.hpp"

#include <unordered_map>

namespace {
	constexpr int LEVEL_EVENT_COMPOSTER_FILL = 1500, READY = 8;

	const std::unordered_map<std::string, float>& compostables() {
		static const std::unordered_map<std::string, float> TABLE = [] {
			std::unordered_map<std::string, float> table;
			auto add = [&](float chance, std::initializer_list<const char*> items) {
				for (const char* item : items) table[std::string("minecraft:") + item] = chance;
			};
			add(0.3F, {"jungle_leaves",	  "oak_leaves",			"spruce_leaves",	   "dark_oak_leaves",	 "pale_oak_leaves",	   "acacia_leaves",
					   "cherry_leaves",	  "birch_leaves",		"azalea_leaves",	   "mangrove_leaves",	 "oak_sapling",		   "spruce_sapling",
					   "birch_sapling",	  "jungle_sapling",		"acacia_sapling",	   "cherry_sapling",	 "dark_oak_sapling",	   "pale_oak_sapling",
					   "mangrove_propagule", "beetroot_seeds",	"dried_kelp",		   "short_grass",		 "kelp",			   "melon_seeds",
					   "pumpkin_seeds",	  "sweet_berries",		"glow_berries",		   "wheat_seeds",		 "moss_carpet",		   "pale_moss_carpet",
					   "pale_hanging_moss", "pink_petals",		"wildflowers",		   "leaf_litter",		 "small_dripleaf",	   "hanging_roots",
					   "mangrove_roots",	  "torchflower_seeds", "pitcher_pod",		   "bush",				 "short_dry_grass",	   "cactus_flower"});
			add(0.5F, {"dried_kelp_block", "tall_grass", "flowering_azalea_leaves", "cactus", "sugar_cane", "vine", "nether_sprouts", "weeping_vines",
					   "twisting_vines", "melon_slice", "glow_lichen", "tall_dry_grass", "firefly_bush"});
			add(0.65F, {"seagrass",	 "moss_block",		"pale_moss_block", "big_dripleaf",	  "lily_pad",	   "pumpkin",		  "carved_pumpkin",
						"melon",	 "apple",			"beetroot",		   "carrot",		  "cocoa_beans",   "potato",		  "wheat",
						"brown_mushroom", "red_mushroom", "mushroom_stem", "crimson_fungus",  "warped_fungus", "warped_roots",	  "crimson_roots",
						"shroomlight", "dandelion",		"poppy",		   "blue_orchid",	  "allium",		   "azure_bluet",	  "red_tulip",
						"orange_tulip", "white_tulip",	"pink_tulip",	   "oxeye_daisy",	  "cornflower",	   "lily_of_the_valley", "wither_rose",
						"open_eyeblossom", "closed_eyeblossom", "fern", "sunflower",	  "lilac",		   "rose_bush",		  "peony",
						"large_fern", "spore_blossom",	"azalea",		   "sea_pickle",	  "torchflower",   "pitcher_plant",	  "nether_wart"});
			add(0.85F, {"hay_block", "brown_mushroom_block", "red_mushroom_block", "nether_wart_block", "warped_wart_block", "flowering_azalea", "bread",
						"baked_potato", "cookie"});
			add(1.0F, {"cake", "pumpkin_pie"});
			return table;
		}();
		return TABLE;
	}

	void setLevel(Level& level, const BlockPos& pos, int value) {
		int state = level.gameData().withProperty(level.getBlockState(pos), "level", std::to_string(value));
		if (state >= 0) level.setBlock(pos, state, Level::UPDATE_ALL);
	}
} // namespace

float ComposterBlock::compostChance(const std::string& item) {
	auto it = compostables().find(item);
	return it == compostables().end() ? 0.0F : it->second;
}

int ComposterBlock::levelOf(Level& level, const BlockPos& pos) {
	std::string value = level.gameData().getProperty(level.getBlockState(pos), "level");
	return value.empty() ? -1 : std::stoi(value);
}

bool ComposterBlock::insert(Level& level, const BlockPos& pos, const std::string& item) {
	int	  current = levelOf(level, pos);
	float chance  = compostChance(item);
	if (current < 0 || current >= 7 || chance <= 0.0F) return false;
	bool added = (current == 0 && chance > 0.0F) || level.random().nextDouble() < chance;
	if (added) {
		setLevel(level, pos, current + 1);
		if (current + 1 == 7) level.scheduleTick(pos, level.blocks().blockOf(level.getBlockState(pos)), 20);
	}
	level.levelEvent(nullptr, LEVEL_EVENT_COMPOSTER_FILL, pos, added ? 1 : 0); // The sound and particles
	return added;
}

UseResult ComposterBlock::useItemOn(Level& level, const BlockPos& pos, int, Player& player, int hand, const BlockHit&) const {
	ItemStack&	stack = player.inventory().getMutable(player.handSlot(hand));
	int			current = levelOf(level, pos);
	std::string item	= stack.isEmpty() ? "" : _ctx->data.getStaticName("minecraft:item", stack.item);
	if (current < 0 || current >= 8 || compostChance(item) <= 0.0F) return UseResult::TryWithEmptyHand;
	if (current < 7) {
		insert(level, pos, item);
		if (player.getGameMode() != GameMode::Creative) stack.shrink(1);
	}
	return UseResult::Success;
}

// extractProduce: a ready composter gives bone meal
bool ComposterBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player&) const {
	if (levelOf(level, pos) != READY) return false;
	JavaRandom& random = level.random();
	double		x = pos.x + 0.5 + random.nextFloat() * 0.7F * 0.5 - 0.175, y = pos.y + 1.01 + random.nextFloat() * 0.7F * 0.5 - 0.175;
	double		z = pos.z + 0.5 + random.nextFloat() * 0.7F * 0.5 - 0.175;
	if (auto item = ItemEntity::create(level, {x, y, z}, ItemStack(level.gameData().getStaticId("minecraft:item", "minecraft:bone_meal"), 1))) {
		level.entities().add(std::move(item));
	}
	setLevel(level, pos, 0);
	level.playSound(nullptr, pos, "minecraft:block.composter.empty", Level::SoundSource::Blocks);
	return true;
}

void ComposterBlock::tick(Level& level, const BlockPos& pos, int) const {
	if (levelOf(level, pos) != 7) return;
	setLevel(level, pos, READY);
	level.playSound(nullptr, pos, "minecraft:block.composter.ready", Level::SoundSource::Blocks);
}

namespace {
	// InputContainer / OutputContainer: a slot that becomes compost or comes out as bone meal
	class ComposterContainer : public Container {
	  public:
		ComposterContainer(Level& level, const BlockPos& pos, bool output) : _level(level), _pos(pos), _output(output) {
			if (output) _slot = ItemStack(level.gameData().getStaticId("minecraft:item", "minecraft:bone_meal"), 1);
		}
		int		   size() const override { return 1; }
		ItemStack& item(int) override { return _slot; }
		int		   maxStackSize() const override { return 1; }
		bool	   isWorldly() const override { return true; }
		std::vector<int> slotsForFace(Direction face) override {
			if (_output) return face == Direction::Down ? std::vector<int>{0} : std::vector<int>{};
			return face == Direction::Up ? std::vector<int>{0} : std::vector<int>{};
		}
		bool canPlaceItemThroughFace(int, const ItemStack& stack, const Direction* face) override {
			return !_output && !_changed && face && *face == Direction::Up && !stack.isEmpty() &&
				   ComposterBlock::compostChance(_level.gameData().getStaticName("minecraft:item", stack.item)) > 0.0F;
		}
		bool canTakeItemThroughFace(int, const ItemStack&, Direction face) override { return _output && face == Direction::Down; }
		bool canPlaceItem(int, const ItemStack&) const override { return !_output; }
		void setChanged() override {
			if (_changed) return;
			if (_output && _slot.isEmpty()) {
				// The bone meal was taken: empty again
				int state = _level.gameData().withProperty(_level.getBlockState(_pos), "level", "0");
				if (state >= 0) _level.setBlock(_pos, state, Level::UPDATE_ALL);
				_changed = true;
			} else if (!_output && !_slot.isEmpty()) {
				ComposterBlock::insert(_level, _pos, _level.gameData().getStaticName("minecraft:item", _slot.item));
				_slot	 = ItemStack();
				_changed = true;
			}
		}

	  private:
		Level&	  _level;
		BlockPos  _pos;
		bool	  _output, _changed = false;
		ItemStack _slot;
	};
} // namespace

std::shared_ptr<Container> ComposterBlock::container(Level& level, const BlockPos& pos) {
	int current = levelOf(level, pos);
	if (current < 0 || current == 7) return nullptr;
	return std::make_shared<ComposterContainer>(level, pos, current == READY);
}

// ===== Beehives =====

bool BeehiveBlock::isSmokey(Level& level, const BlockPos& pos) {
	const GameData& data = level.gameData();
	auto			litCampfire = [&](int state) {
		   std::string name = data.getStaticName("minecraft:block", level.blocks().blockOf(state));
		   return (name == "minecraft:campfire" || name == "minecraft:soul_campfire") && data.getProperty(state, "lit") == "true";
	};
	for (int i = 1; i <= 5; i++) {
		int state = level.getBlockState(pos.relative(Direction::Down, i));
		if (litCampfire(state)) return true;
		// A solid block in the way: the smoke goes no further than the one right under it
		if (data.getStateProperties(state).blocksMotion) return litCampfire(level.getBlockState(pos.relative(Direction::Down, i + 1)));
	}
	return false;
}

UseResult BeehiveBlock::useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit&) const {
	const GameData& data  = level.gameData();
	ItemStack&		stack = player.inventory().getMutable(player.handSlot(hand));
	if (stack.isEmpty() || data.getProperty(state, "honey_level") != "5") return UseResult::TryWithEmptyHand;
	std::string item = data.getStaticName("minecraft:item", stack.item);
	if (item == "minecraft:shears") {
		level.playSound(nullptr, pos, "minecraft:block.beehive.shear", Level::SoundSource::Neutral);
		// dropHoneycomb: 3 honeycombs
		if (auto drop = ItemEntity::create(level, {pos.x + 0.5, pos.y + 1.1, pos.z + 0.5}, ItemStack(data.getStaticId("minecraft:item", "minecraft:honeycomb"), 3))) {
			level.entities().add(std::move(drop));
		}
		ItemDamage::hurtAndBreak(level, stack, 1, &player, player.handSlot(hand));
	} else if (item == "minecraft:glass_bottle") {
		level.playSound(nullptr, pos, "minecraft:item.bottle.fill", Level::SoundSource::Neutral);
		ItemStack result = ItemUse::filledResult(level, player, stack, ItemStack(data.getStaticId("minecraft:item", "minecraft:honey_bottle"), 1));
		player.inventory().set(player.handSlot(hand), std::move(result));
	} else {
		return UseResult::TryWithEmptyHand;
	}
	// resetHoneyLevel, and the bees' anger unless smoked
	int empty = data.withProperty(state, "honey_level", "0");
	if (empty >= 0) level.setBlock(pos, empty, Level::UPDATE_ALL);
	if (!isSmokey(level, pos)) {
		if (auto* hive = dynamic_cast<BeehiveBlockEntity*>(level.getBlockEntity(pos))) hive->releaseAll(level, &player);
		// angerNearbyBees: the bees around without a target go for the player
		AABB area{pos.x - 8.0, pos.y - 6.0, pos.z - 8.0, pos.x + 9.0, pos.y + 7.0, pos.z + 9.0};
		for (Actor* actor : level.actorsIn(area)) {
			if (auto* bee = dynamic_cast<Bee*>(actor); bee && bee->isAlive() && !bee->getTarget()) bee->setTarget(&player);
		}
	}
	return UseResult::Success;
}
