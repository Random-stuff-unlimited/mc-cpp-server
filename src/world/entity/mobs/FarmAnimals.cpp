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
	const GameData& data = _level.gameData();
	auto			dye	 = [&](int color) { return ItemStack(data.getStaticId("minecraft:item", std::string("minecraft:") + COLORS[color] + "_dye"), 1); };
	CraftingInput	input  = CraftingInput::ofPositioned(2, 1, {dye(color()), dye(other->color())}).input;
	int				mixed  = -1;
	if (const Recipe* recipe = _level.recipes().getRecipeFor(RecipeType::Crafting, input)) {
		ItemStack result = recipe->assemble(input);
		for (int c = 0; c < 16 && !result.isEmpty(); c++) {
			if (data.getStaticName("minecraft:item", result.item) == std::string("minecraft:") + COLORS[c] + "_dye") mixed = c;
		}
	}
	lamb->setColor(mixed >= 0 ? mixed : (_level.random().nextBoolean() ? color() : other->color()));
	return child;
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
