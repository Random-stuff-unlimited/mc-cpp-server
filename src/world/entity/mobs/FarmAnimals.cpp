#include "world/entity/mobs/FarmAnimals.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/Variants.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/ItemDamage.hpp"
#include "world/item/ItemUse.hpp"
#include "world/item/Recipes.hpp"
#include "world/item/Components.hpp"
#include "world/blockentity/StorageEntities.hpp"

namespace {
	constexpr int SAVE_VERSION = 1;
	// EntityDataSerializers ids of the variant holders
	constexpr int SERIALIZER_COW_VARIANT = 22, SERIALIZER_PIG_VARIANT = 26, SERIALIZER_CHICKEN_VARIANT = 27;
	constexpr const char* COLORS[16] = {"white", "orange", "magenta",	"light_blue", "yellow", "lime",  "pink",	"gray",
										"light_gray", "cyan",  "purple", "blue",	   "brown",	 "green", "red", "black"};
	constexpr int WHITE = 0, PINK = 6, GRAY = 7, LIGHT_GRAY = 8, BROWN = 12, BLACK = 15;

	std::function<bool(const ItemStack&)> itemTag(Level& level, const std::string& tag) {
		return [&level, tag](const ItemStack& stack) { return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item); };
	}
	std::function<bool(const ItemStack&)> item(Level& level, const std::string& name) {
		int id = level.gameData().getStaticId("minecraft:item", name);
		return [id](const ItemStack& stack) { return !stack.isEmpty() && stack.item == id; };
	}
	bool isItem(Level& level, const ItemStack& stack, const char* name) {
		return !stack.isEmpty() && level.gameData().getStaticName("minecraft:item", stack.item) == name;
	}

	// SheepColorSpawnRules.getSheepColor: by the biome's climate, mostly the common color (white, brown in warm biomes,
	// black in cold ones), 1 in 500 of those pink
	int randomSheepColor(Level& level, const BlockPos& pos) {
		const GameData& data  = level.gameData();
		int				biome = level.getBiomeId(pos);
		struct Entry {
			int color, weight;
		};
		const Entry* entries;
		int			 common;
		static constexpr Entry TEMPERATE[4] = {{BLACK, 5}, {GRAY, 5}, {LIGHT_GRAY, 5}, {BROWN, 3}};
		static constexpr Entry WARM[4]		= {{GRAY, 5}, {LIGHT_GRAY, 5}, {WHITE, 5}, {BLACK, 3}};
		static constexpr Entry COLD[4]		= {{LIGHT_GRAY, 5}, {GRAY, 5}, {WHITE, 5}, {BROWN, 3}};
		if (data.isInTag("minecraft:worldgen/biome", "minecraft:spawns_warm_variant_farm_animals", biome)) {
			entries = WARM;
			common	= BROWN;
		} else if (data.isInTag("minecraft:worldgen/biome", "minecraft:spawns_cold_variant_farm_animals", biome)) {
			entries = COLD;
			common	= BLACK;
		} else {
			entries = TEMPERATE;
			common	= WHITE;
		}
		JavaRandom& random = level.random();
		int			r	   = random.nextInt(100); // 18 for the singles, then 82 for the common colors
		for (int i = 0; i < 4; i++) {
			r -= entries[i].weight;
			if (r < 0) return entries[i].color;
		}
		return random.nextInt(500) < 499 ? common : PINK;
	}
} // namespace

const char* dyeColorName(int color) { return color >= 0 && color < 16 ? COLORS[color] : "white"; }

// ===================== VariantAnimal =====================

VariantAnimal::VariantAnimal(Level& level, int typeId, std::string registry, int dataId, int serializer)
	: Animal(level, typeId), _registry(std::move(registry)), _dataId(dataId), _serializer(serializer) {
	_defaultVariant = std::max(0, Variants::id(level, _registry, "minecraft:temperate"));
	_variant		= _defaultVariant;
}

void VariantAnimal::setVariant(int variant) {
	if (variant < 0 || variant == _variant) return;
	_variant = variant;
	markData(_dataId);
}

std::shared_ptr<SpawnGroupData> VariantAnimal::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	setVariant(Variants::selectToSpawn(_level, _registry, blockPosition()));
	return Animal::finalizeSpawn(difficulty, reason, group);
}

std::unique_ptr<Mob> VariantAnimal::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = Animal::getBreedOffspring(partner);
	auto*				 baby  = dynamic_cast<VariantAnimal*>(child.get());
	auto*				 other = dynamic_cast<VariantAnimal*>(&partner);
	if (baby && other) baby->setVariant(_random.nextBoolean() ? _variant : other->_variant);
	return child;
}

std::string VariantAnimal::lootComponent(const std::string& component) const {
	// "minecraft:cow_variant" -> "minecraft:cow/variant"
	std::string key = _registry.substr(0, _registry.size() - std::string("_variant").size()) + "/variant";
	return component == key ? Variants::name(_level, _registry, _variant) : Animal::lootComponent(component);
}

void VariantAnimal::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, _dataId, onlyNonDefault, _variant == _defaultVariant)) {
		buf.writeUByte(static_cast<uint8_t>(_dataId));
		buf.writeVarInt(_serializer);
		buf.writeVarInt(_variant); // ByteBufCodecs.holderRegistry: the id in the synced registry
	}
}

void VariantAnimal::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeString(Variants::name(_level, _registry, _variant));
}

void VariantAnimal::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown variant data version");
	int variant = Variants::id(_level, _registry, buf.readString());
	_variant	= variant >= 0 ? variant : _defaultVariant;
	_dirtyData	= 0;
}

// ===================== Mooshroom =====================

namespace {
	// The flowers' SuspiciousEffectHolder effects (FlowerBlock: seconds * 20 ticks)
	std::optional<std::pair<MobEffects::Effect, int>> flowerStewEffect(const std::string& item) {
		static const std::vector<std::tuple<const char*, MobEffects::Effect, float>> flowers = {
				{"minecraft:dandelion", MobEffects::Saturation, 0.35F},		  {"minecraft:torchflower", MobEffects::NightVision, 5.0F},
				{"minecraft:poppy", MobEffects::NightVision, 5.0F},			  {"minecraft:blue_orchid", MobEffects::Saturation, 0.35F},
				{"minecraft:allium", MobEffects::FireResistance, 3.0F},		  {"minecraft:azure_bluet", MobEffects::Blindness, 11.0F},
				{"minecraft:red_tulip", MobEffects::Weakness, 7.0F},		  {"minecraft:orange_tulip", MobEffects::Weakness, 7.0F},
				{"minecraft:white_tulip", MobEffects::Weakness, 7.0F},		  {"minecraft:pink_tulip", MobEffects::Weakness, 7.0F},
				{"minecraft:oxeye_daisy", MobEffects::Regeneration, 7.0F},	  {"minecraft:cornflower", MobEffects::JumpBoost, 5.0F},
				{"minecraft:wither_rose", MobEffects::Wither, 7.0F},		  {"minecraft:lily_of_the_valley", MobEffects::Poison, 11.0F},
				{"minecraft:open_eyeblossom", MobEffects::Blindness, 11.0F}, {"minecraft:closed_eyeblossom", MobEffects::Nausea, 7.0F},
		};
		for (const auto& [name, effect, seconds] : flowers) {
			if (item == name) return std::make_pair(effect, static_cast<int>(std::floor(seconds * 20.0F)));
		}
		return std::nullopt;
	}
} // namespace

void Mooshroom::setVariant(int variant) {
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_TYPE);
}

// AbstractCow.registerGoals
void Mooshroom::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 2.0));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.25, itemTag(_level, "minecraft:cow_food"), false));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
}

// MushroomCow.getWalkTargetValue: mycelium, else the light
float Mooshroom::getWalkTargetValue(const BlockPos& pos) {
	int below = _level.gameData().getBlocks().blockOf(_level.getBlockState(pos.below()));
	if (below == _level.gameData().getStaticId("minecraft:block", "minecraft:mycelium")) return 10.0F;
	return _level.getPathfindingCostFromLightLevels(pos);
}

bool Mooshroom::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	const GameData&	 data  = _level.gameData();
	if (isItem(_level, stack, "minecraft:bowl") && !isBaby()) {
		// A bowl: mushroom stew, or the suspicious stew of the flower it ate
		bool	  suspicious = _hasStewEffects;
		ItemStack stew(data.getStaticId("minecraft:item", suspicious ? "minecraft:suspicious_stew" : "minecraft:mushroom_stew"), 1);
		if (suspicious) {
			Components::set(stew, data, "minecraft:suspicious_stew_effects", Components::encodeStewEffects(_stewEffects));
			_stewEffects.clear();
			_hasStewEffects = false;
		}
		ItemStack held = ItemUse::filledResult(_level, player, stack, std::move(stew));
		player.inventory().set(player.handSlot(hand), std::move(held));
		playSound(suspicious ? "minecraft:entity.mooshroom.suspicious_milk" : "minecraft:entity.mooshroom.milk", 1.0F, 1.0F);
		return true;
	}
	if (isItem(_level, stack, "minecraft:shears") && readyForShearing()) {
		shear();
		ItemDamage::hurtAndBreak(_level, player.inventory().getMutable(player.handSlot(hand)), 1, &player, player.handSlot(hand));
		return true;
	}
	if (_variant == Brown && !stack.isEmpty()) {
		if (auto effect = flowerStewEffect(data.getStaticName("minecraft:item", stack.item))) {
			if (_hasStewEffects) {
				// Already fed: smoke. Level.addParticle does nothing on the server, only its random draws are made
				for (int i = 0; i < 6; i++) _random.nextDouble();
			} else {
				if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1); // ItemStack.consume
				for (int i = 0; i < 12; i++) _random.nextDouble(); // The effect particles' draws
				_stewEffects	= {{effect->first, effect->second}};
				_hasStewEffects = true;
				playSound("minecraft:entity.mooshroom.eat", 2.0F, 1.0F);
			}
			return true;
		}
	}
	return Animal::mobInteract(player, hand);
}

void Mooshroom::shear() {
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.mooshroom.shear", Level::SoundSource::Players, 1.0F, 1.0F);
	// The mushrooms drop where it stood, one by one, from the top of the mooshroom
	Vec3				   at = _position;
	float				   top = _height;
	std::vector<ItemStack> drops = rollLootTable("minecraft:shearing/mooshroom");
	convertTo(_level.gameData().getStaticId("minecraft:entity_type", "minecraft:cow"), false, false, [&](Mob&) {
		sendParticles(_level, "minecraft:explosion", at.x, at.y + top * 0.5, at.z, 1, 0.0, 0.0, 0.0, 0.0);
		for (ItemStack& stack : drops) {
			for (int i = 0; i < stack.count; i++) {
				auto item = ItemEntity::create(_level, {at.x, at.y + top, at.z}, stack.copyWithCount(1));
				_level.entities().add(std::move(item));
			}
		}
	});
}

// MushroomCow.getOffspringVariant: one parent's, a mutation (1/1024) when both are the same
std::unique_ptr<Mob> Mooshroom::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = Animal::getBreedOffspring(partner);
	auto*				 calf  = dynamic_cast<Mooshroom*>(child.get());
	auto*				 other = dynamic_cast<Mooshroom*>(&partner);
	if (!calf || !other) return child;
	int variant;
	if (_variant == other->_variant && _random.nextInt(1024) == 0) {
		variant = _variant == Brown ? Red : Brown;
	} else {
		variant = _random.nextBoolean() ? _variant : other->_variant;
	}
	calf->setVariant(variant);
	return child;
}

std::string Mooshroom::lootComponent(const std::string& component) const {
	if (component == "minecraft:mooshroom/variant") return _variant == Brown ? "brown" : "red";
	return Animal::lootComponent(component);
}

void Mooshroom::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_TYPE, onlyNonDefault, _variant == Red)) writeIntData(buf, DATA_TYPE, _variant);
}

void Mooshroom::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(_variant);
	buf.writeBool(_hasStewEffects);
	buf.writeVarInt(static_cast<int32_t>(_stewEffects.size()));
	for (const auto& [effect, duration] : _stewEffects) {
		buf.writeVarInt(effect);
		buf.writeVarInt(duration);
	}
}

void Mooshroom::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown mooshroom data version");
	_variant		= buf.readVarInt() == Brown ? Brown : Red;
	_hasStewEffects = buf.readBool();
	_stewEffects.clear();
	for (int i = buf.readVarInt(); i > 0; i--) {
		int effect = buf.readVarInt();
		_stewEffects.emplace_back(effect, buf.readVarInt());
	}
	_dirtyData = 0;
}

// ===================== Cow =====================

Cow::Cow(Level& level, int typeId) : VariantAnimal(level, typeId, "minecraft:cow_variant", 17, SERIALIZER_COW_VARIANT) {}

void Cow::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 2.0));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.25, itemTag(_level, "minecraft:cow_food"), false));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
}

// AbstractCow.mobInteract: milked with a bucket
bool Cow::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isItem(_level, stack, "minecraft:bucket") && !isBaby()) {
		_level.playSoundAt(&player, player.getX(), player.getY(), player.getZ(), "minecraft:entity.cow.milk", Level::SoundSource::Players, 1.0F, 1.0F);
		ItemStack milk(_level.gameData().getStaticId("minecraft:item", "minecraft:milk_bucket"), 1);
		ItemStack held = ItemUse::filledResult(_level, player, stack, std::move(milk));
		player.inventory().set(player.handSlot(hand), std::move(held));
		return true;
	}
	return VariantAnimal::mobInteract(player, hand);
}

// ===================== Pig =====================

Pig::Pig(Level& level, int typeId) : VariantAnimal(level, typeId, "minecraft:pig_variant", 18, SERIALIZER_PIG_VARIANT) {}

bool Pig::mobInteract(Player& player, int hand) {
	bool food = isFood(player.getStackInHand(hand));
	if (!food && isSaddled() && !hasRider() && !player.isShiftKeyDown()) {
		startRiding(player);
		return true;
	}
	if (VariantAnimal::mobInteract(player, hand)) return true;
	return isAlive() && !isBaby() && equipFromPlayer(player, hand, EquipmentSlot::Saddle);
}

bool Pig::isControlledByRider() {
	Player* player = rider();
	if (!player || !isSaddled()) return false;
	int stick = _level.gameData().getStaticId("minecraft:item", "minecraft:carrot_on_a_stick");
	return player->getStackInHand(0).item == stick || player->getStackInHand(1).item == stick;
}

void Pig::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.25));
	_goalSelector.addGoal(3, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<TemptGoal>(*this, 1.2, item(_level, "minecraft:carrot_on_a_stick"), false));
	_goalSelector.addGoal(4, std::make_unique<TemptGoal>(*this, 1.2, itemTag(_level, "minecraft:pig_food"), false));
	_goalSelector.addGoal(5, std::make_unique<FollowParentGoal>(*this, 1.1));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
}

// ===================== Chicken =====================

Chicken::Chicken(Level& level, int typeId) : VariantAnimal(level, typeId, "minecraft:chicken_variant", 17, SERIALIZER_CHICKEN_VARIANT) {
	_eggTime = _random.nextInt(6000) + 6000;
	setPathfindingMalus(PathType::Water, 0.0F);
}

void Chicken::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.4));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.0, itemTag(_level, "minecraft:chicken_food"), false));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 1.1));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
}

void Chicken::aiStep() {
	VariantAnimal::aiStep();
	// The wings flap (the client animates them): it falls slowly
	if (!_onGround && _delta.y < 0.0) _delta = _delta.multiply(1.0, 0.6, 1.0);
	if (isAlive() && !isBaby() && !_isChickenJockey && --_eggTime <= 0) {
		// dropFromGiftLootTable(CHICKEN_LAY): its variant's egg
		bool laid = false;
		for (ItemStack& egg : rollLootTable("minecraft:gameplay/chicken_lay")) laid |= spawnAtLocation(std::move(egg)) != nullptr;
		if (laid) playSound("minecraft:entity.chicken.egg", 1.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
		_eggTime = _random.nextInt(6000) + 6000;
	}
}

void Chicken::save(Buffer& buf) const {
	VariantAnimal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_isChickenJockey);
	buf.writeInt(_eggTime);
}

void Chicken::load(Buffer& buf) {
	VariantAnimal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown chicken data version");
	_isChickenJockey = buf.readBool();
	_eggTime		 = buf.readInt();
}

// ===================== Sheep =====================

void Sheep::setColor(int color) {
	uint8_t wool = static_cast<uint8_t>((_wool & 240) | (color & 15));
	if (wool == _wool) return;
	_wool = wool;
	markData(DATA_WOOL);
}

void Sheep::setSheared(bool sheared) {
	uint8_t wool = static_cast<uint8_t>(sheared ? (_wool | 16) : (_wool & ~16));
	if (wool == _wool) return;
	_wool = wool;
	markData(DATA_WOOL);
}

void Sheep::registerGoals() {
	auto eat	  = std::make_unique<EatBlockGoal>(*this);
	_eatBlockGoal = eat.get();
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.25));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 1.1, itemTag(_level, "minecraft:sheep_food"), false));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 1.1));
	_goalSelector.addGoal(5, std::move(eat));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
}

void Sheep::shear() {
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.sheep.shear", Level::SoundSource::Players, 1.0F, 1.0F);
	// dropFromShearingLootTable(SHEAR_SHEEP): each wool alone, 1 block up, pushed a little
	for (ItemStack& stack : rollLootTable("minecraft:shearing/sheep")) {
		for (int i = 0; i < stack.count; i++) {
			ItemStack one = stack;
			one.count	  = 1;
			if (ItemEntity* item = spawnAtLocation(std::move(one), 1.0F)) {
				double dx = (_random.nextFloat() - _random.nextFloat()) * 0.1F, dy = _random.nextFloat() * 0.05F;
				double dz = (_random.nextFloat() - _random.nextFloat()) * 0.1F;
				item->setDeltaMovement(item->deltaMovement() + Vec3{dx, dy, dz});
			}
		}
	}
	setSheared(true);
}

bool Sheep::mobInteract(Player& player, int hand) {
	if (isItem(_level, player.getStackInHand(hand), "minecraft:shears")) {
		if (!readyForShearing()) return false;
		shear();
		ItemDamage::hurtAndBreak(_level, player.inventory().getMutable(player.handSlot(hand)), 1, &player, player.handSlot(hand));
		return true;
	}
	return Animal::mobInteract(player, hand);
}

void Sheep::ate() {
	Animal::ate();
	setSheared(false);
	if (isBaby()) ageUp(60);
}

std::shared_ptr<SpawnGroupData> Sheep::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	setColor(randomSheepColor(_level, blockPosition()));
	return Animal::finalizeSpawn(difficulty, reason, group);
}

// Sheep.getBreedOffspring: the color the parents' dyes craft into (DyeColor.getMixedColor), else one of theirs
std::unique_ptr<Mob> Sheep::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = Animal::getBreedOffspring(partner);
	auto*				 lamb  = dynamic_cast<Sheep*>(child.get());
	auto*				 other = dynamic_cast<Sheep*>(&partner);
	if (!lamb || !other) return child;
	lamb->setColor(mixedDyeColor(_level, color(), other->color()));
	return child;
}

int mixedDyeColor(Level& level, int a, int b) {
	const GameData& data = level.gameData();
	auto			dye	 = [&](int color) { return ItemStack(data.getStaticId("minecraft:item", std::string("minecraft:") + COLORS[color] + "_dye"), 1); };
	CraftingInput	input = CraftingInput::ofPositioned(2, 1, {dye(a), dye(b)}).input;
	if (const Recipe* recipe = level.recipes().getRecipeFor(RecipeType::Crafting, input)) {
		int color = dyeColorOfItem(level, recipe->assemble(input));
		if (color >= 0) return color;
	}
	return level.random().nextBoolean() ? a : b;
}

int dyeColorOfItem(Level& level, const ItemStack& stack) {
	if (stack.isEmpty()) return -1;
	const std::string& name = level.gameData().getStaticName("minecraft:item", stack.item);
	for (int c = 0; c < 16; c++) {
		if (name == std::string("minecraft:") + COLORS[c] + "_dye") return c;
	}
	return -1;
}

std::string Sheep::lootComponent(const std::string& component) const {
	return component == "minecraft:sheep/color" ? COLORS[color()] : Animal::lootComponent(component);
}

bool Sheep::lootTypeSpecific(const nlohmann::json& predicate) const {
	if (predicate.value("type", "") != "minecraft:sheep") return false;
	return !predicate.contains("sheared") || predicate["sheared"].get<bool>() == isSheared();
}

void Sheep::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_WOOL, onlyNonDefault, _wool == 0)) writeByteData(buf, DATA_WOOL, _wool);
}

void Sheep::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(_wool);
}

void Sheep::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown sheep data version");
	_wool	   = buf.readUByte();
	_dirtyData = 0;
}
