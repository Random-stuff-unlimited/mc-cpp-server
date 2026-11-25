#include "world/item/Enchantments.hpp"
#include "world/entity/LightningBolt.hpp"
#include "world/inventory/HorseMenu.hpp"
#include "world/entity/mobs/Horses.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"

#include <cmath>

namespace {
	constexpr int	 SAVE_VERSION = 1;
	constexpr int	 EVENT_TAMING_FAILED = 6, EVENT_TAMING_SUCCEEDED = 7;
	constexpr double MIN_SPEED = (0.45F) * 0.25, MAX_SPEED = (0.45F + 0.9) * 0.25;
	constexpr double MIN_JUMP = 0.4F, MAX_JUMP = 0.4F + 0.6;
	constexpr double MIN_HEALTH = 15.0, MAX_HEALTH = 30.0;

	struct HorseGroupData : AgeableMobGroupData {
		int variant;
		explicit HorseGroupData(int v) : AgeableMobGroupData(true, 0.05F), variant(v) {}
	};

	// RandomStandGoal: rears now and then
	class RandomStandGoal : public Goal {
	  public:
		explicit RandomStandGoal(AbstractHorse& horse) : _horse(horse), _nextStand(-horse.ambientStandInterval()) {}
		void start() override {
			_horse.standIfPossible();
			std::string sound = _horse.ambientStandSound();
			if (!sound.empty()) _horse.playSound(sound, 0.8F, 1.0F);
		}
		bool canContinueToUse() override { return false; }
		bool canUse() override {
			_nextStand++;
			if (_nextStand > 0 && _horse.random().nextInt(1000) < _nextStand) {
				_nextStand = -_horse.ambientStandInterval();
				return !_horse.isImmobile() && _horse.random().nextInt(10) == 0;
			}
			return false;
		}
		bool requiresUpdateEveryTick() override { return true; }

	  private:
		AbstractHorse& _horse;
		int			   _nextStand;
	};

	// AbstractHorse.createOffspringAttribute: around the parents' mean, bounced back into the range
	double createOffspringAttribute(double a, double b, double min, double max, JavaRandom& random) {
		a			  = std::clamp(a, min, max);
		b			  = std::clamp(b, min, max);
		double margin = 0.15 * (max - min);
		double spread = std::abs(a - b) + margin * 2.0;
		double mean	  = (a + b) / 2.0;
		double roll	  = (random.nextDouble() + random.nextDouble() + random.nextDouble()) / 3.0 - 0.5;
		double value  = mean + spread * roll;
		if (value > max) return max - (value - max);
		if (value < min) return min + (min - value);
		return value;
	}
} // namespace

// ===================== RunAroundLikeCrazyGoal =====================

bool RunAroundLikeCrazyGoal::canUse() {
	if (_horse.isTamed() || !_horse.hasRider()) return false;
	std::optional<Vec3> pos = RandomPos::defaultPos(_horse, 5, 4);
	if (!pos) return false;
	_posX = pos->x;
	_posY = pos->y;
	_posZ = pos->z;
	return true;
}

void RunAroundLikeCrazyGoal::start() { _horse.navigation().moveTo(_posX, _posY, _posZ, _speedModifier); }

bool RunAroundLikeCrazyGoal::canContinueToUse() { return !_horse.isTamed() && !_horse.navigation().isDone() && _horse.hasRider(); }

void RunAroundLikeCrazyGoal::tick() {
	if (_horse.isTamed() || _horse.random().nextInt(adjustedTickDelay(50)) != 0) return;
	Player* player = _horse.rider();
	if (!player) return;
	int max = _horse.maxTemper();
	if (max > 0 && _horse.random().nextInt(max) < _horse.temper()) {
		_horse.tameWithName(*player);
		return;
	}
	_horse.modifyTemper(5);
	_horse.ejectRider();
	_horse.makeMad();
	_horse.broadcastHorseEvent(EVENT_TAMING_FAILED);
}

// ===================== AbstractHorse =====================

AbstractHorse::AbstractHorse(Level& level, int typeId) : Animal(level, typeId) {
	// Its saddle and body armor always drop
	setGuaranteedDrop(EquipmentSlot::Saddle);
	setGuaranteedDrop(EquipmentSlot::Body);
}

void AbstractHorse::openInventory(Player& player) {
	// AbstractHorse.openCustomInventoryScreen: for its rider, or anyone once tamed
	if (!isTamed() && rider() != &player) return;
	Menus::openHorseInventory(player, _level, *this);
}

void AbstractHorse::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.2));
	_goalSelector.addGoal(1, std::make_unique<RunAroundLikeCrazyGoal>(*this, 1.2));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0, "AbstractHorse"));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.7));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	if (canPerformRearing()) _goalSelector.addGoal(9, std::make_unique<RandomStandGoal>(*this));
	addBehaviourGoals();
}

void AbstractHorse::addBehaviourGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(
									 *this, 1.25,
									 [this](const ItemStack& stack) {
										 return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:horse_tempt_items", stack.item);
									 },
									 false));
}

void AbstractHorse::setFlag(int flag, bool value) {
	uint8_t flags = static_cast<uint8_t>(value ? _horseFlags | flag : _horseFlags & ~flag);
	if (flags == _horseFlags) return;
	_horseFlags = flags;
	markData(DATA_FLAGS);
}

int AbstractHorse::modifyTemper(int amount) {
	_temper = std::clamp(_temper + amount, 0, maxTemper());
	return _temper;
}

void AbstractHorse::setStanding(int ticks) {
	setEating(false);
	setFlag(FLAG_STANDING, true);
	_standCounter = ticks;
}

void AbstractHorse::clearStanding() {
	setFlag(FLAG_STANDING, false);
	_standCounter = 0;
}

void AbstractHorse::standIfPossible() {
	if (canPerformRearing()) setStanding(20);
}

void AbstractHorse::makeMad() {
	if (isStanding()) return;
	standIfPossible();
	playSound(typeSound("angry", ""), soundVolume(), voicePitch());
}

void AbstractHorse::tameWithName(Player& player) {
	_owner = EntityRef(player);
	setTamed(true);
	broadcastEntityEvent(EVENT_TAMING_SUCCEEDED);
}

void AbstractHorse::handleStartJump() {
	standIfPossible();
	playSound(typeSound("jump", "minecraft:entity.horse.jump"), 0.4F, 1.0F);
}

bool AbstractHorse::canParent() { return !hasRider() && isTamed() && !isBaby() && _health >= maxHealth() && isInLove(); }

bool AbstractHorse::isImmobile() const { return Animal::isImmobile() || isEating() || isStanding(); }

bool AbstractHorse::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:horse_food", stack.item);
}

bool AbstractHorse::hurtServer(const Combat::DamageSource& source, float amount) {
	bool hurt = Animal::hurtServer(source, amount);
	if (hurt && _random.nextInt(3) == 0) standIfPossible();
	return hurt;
}

bool AbstractHorse::handleEating(Player& player, const ItemStack& stack) {
	const GameData& data = _level.gameData();
	auto			is	 = [&](const char* name) { return stack.item == data.getStaticId("minecraft:item", name); };
	bool			used = false;
	float			heal = 0.0F;
	int				age = 0, temper = 0;
	if (is("minecraft:wheat")) {
		heal = 2.0F, age = 20, temper = 3;
	} else if (is("minecraft:sugar")) {
		heal = 1.0F, age = 30, temper = 3;
	} else if (is("minecraft:hay_block")) {
		heal = 20.0F, age = 180;
	} else if (is("minecraft:apple") || is("minecraft:carrot")) {
		heal = 3.0F, age = 60, temper = 3;
	} else if (is("minecraft:golden_carrot") || is("minecraft:golden_apple") || is("minecraft:enchanted_golden_apple")) {
		bool carrot = is("minecraft:golden_carrot");
		heal		= carrot ? 4.0F : 10.0F;
		age			= carrot ? 60 : 240;
		temper		= carrot ? 5 : 10;
		if (isTamed() && _age == 0 && !isInLove()) {
			used = true;
			setInLove(&player);
		}
	}
	if (_health < maxHealth() && heal > 0.0F) {
		this->heal(heal);
		used = true;
	}
	if (isBaby() && age > 0) {
		_random.nextDouble(); // The happy villager particle's position
		_random.nextDouble();
		_random.nextDouble();
		ageUp(age);
		used = true;
	}
	if (temper > 0 && (used || !isTamed()) && _temper < maxTemper()) {
		modifyTemper(temper);
		used = true;
	}
	if (used) {
		// eating: the mouth opens
		_mouthCounter = 1;
		setFlag(FLAG_OPEN_MOUTH, true);
		std::string sound = typeSound("eat", "");
		if (!sound.empty()) {
			_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, sound, Level::SoundSource::Neutral, 1.0F,
							   1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
		}
	}
	return used;
}

bool AbstractHorse::fedFood(Player& player, int hand) {
	bool used = handleEating(player, player.getStackInHand(hand));
	if (used && !player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
	return used;
}

void AbstractHorse::doPlayerRide(Player& player) {
	setEating(false);
	clearStanding();
	startRiding(player);
}

bool AbstractHorse::mobInteract(Player& player, int hand) {
	if (hasRider() || isBaby()) return Animal::mobInteract(player, hand);
	if (isTamed() && player.isShiftKeyDown()) {
		openInventory(player);
		return true;
	}
	if (!player.getStackInHand(hand).isEmpty()) {
		if (isTamed() && equipFromPlayer(player, hand, EquipmentSlot::Saddle)) return true;
		if (equipFromPlayer(player, hand, EquipmentSlot::Body)) return true;
	}
	doPlayerRide(player);
	return true;
}

void AbstractHorse::aiStep() {
	_random.nextInt(200); // moveTail
	Animal::aiStep();
	if (!isAlive()) return;
	if (_random.nextInt(900) == 0 && _deathTime == 0) heal(1.0F);
	if (canEatGrass()) {
		if (!isEating() && !hasRider() && _random.nextInt(300) == 0 &&
			_level.blocks().blockOf(_level.getBlockState(blockPosition().below())) == _level.gameData().getStaticId("minecraft:block", "minecraft:grass_block")) {
			setEating(true);
		}
		if (isEating() && ++_eatingCounter > 50) {
			_eatingCounter = 0;
			setEating(false);
		}
	}
}

void AbstractHorse::tick() {
	Animal::tick();
	if (_mouthCounter > 0 && ++_mouthCounter > 30) {
		_mouthCounter = 0;
		setFlag(FLAG_OPEN_MOUTH, false);
	}
	if (_standCounter > 0 && --_standCounter <= 0) clearStanding();
}

void AbstractHorse::setOffspringAttributes(Animal& partner, AbstractHorse& child) {
	auto set = [&](int attribute, double min, double max) {
		AttributeInstance* mine	  = _attributes.getInstance(attribute);
		AttributeInstance* theirs = partner.attributes().getInstance(attribute);
		AttributeInstance* target = child.attributes().getInstance(attribute);
		if (mine && theirs && target) target->setBaseValue(createOffspringAttribute(mine->baseValue(), theirs->baseValue(), min, max, _random));
	};
	set(_ids.maxHealth, MIN_HEALTH, MAX_HEALTH);
	set(_ids.jumpStrength, MIN_JUMP, MAX_JUMP);
	set(_ids.movementSpeed, MIN_SPEED, MAX_SPEED);
}

std::shared_ptr<SpawnGroupData> AbstractHorse::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	randomizeAttributes(_level.random());
	setHealth(maxHealth());
	return Animal::finalizeSpawn(difficulty, reason, group);
}

void AbstractHorse::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _horseFlags == 0)) writeByteData(buf, DATA_FLAGS, _horseFlags);
}

void AbstractHorse::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeUByte(static_cast<uint8_t>(_horseFlags & (FLAG_TAME | FLAG_BRED | FLAG_EATING)));
	buf.writeVarInt(_temper);
	buf.writeBool(_owner.isSet());
	if (_owner.isSet()) buf.writeUUID(_owner.uuid);
}

void AbstractHorse::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown horse data version");
	_horseFlags = buf.readUByte();
	_temper		= buf.readVarInt();
	_owner.clear();
	if (buf.readBool()) {
		_owner.uuid = buf.readUUID();
		_owner.id	= -2;
	}
	_dirtyData = 0;
}

// ===================== Horse =====================

void Horse::randomizeAttributes(JavaRandom& random) {
	if (AttributeInstance* health = _attributes.getInstance(_ids.maxHealth)) health->setBaseValue(generateMaxHealth(random));
	if (AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed)) speed->setBaseValue(generateSpeed(random));
	if (AttributeInstance* jump = _attributes.getInstance(_ids.jumpStrength)) jump->setBaseValue(generateJumpStrength(random));
}

void Horse::setTypeVariant(int variant) {
	if (variant == _typeVariant) return;
	_typeVariant = variant;
	markData(DATA_TYPE_VARIANT);
}

bool Horse::mobInteract(Player& player, int hand) {
	bool inventory = !isBaby() && isTamed() && player.isShiftKeyDown();
	if (!hasRider() && !inventory && !player.getStackInHand(hand).isEmpty()) {
		if (isFood(player.getStackInHand(hand))) return fedFood(player, hand);
		if (!isTamed()) {
			makeMad();
			return true;
		}
	}
	return AbstractHorse::mobInteract(player, hand);
}

bool Horse::canMate(Animal& other) {
	if (&other == this) return false;
	auto* horse = dynamic_cast<AbstractHorse*>(&other);
	if (!horse || !(dynamic_cast<Donkey*>(&other) || dynamic_cast<Horse*>(&other))) return false;
	return canParent() && horse->canParent();
}

std::unique_ptr<Mob> Horse::getBreedOffspring(Animal& partner) {
	if (dynamic_cast<Donkey*>(&partner)) {
		std::unique_ptr<Mob> mule = _level.mobs().create(_level, _level.gameData().getStaticId("minecraft:entity_type", "minecraft:mule"));
		if (auto* child = dynamic_cast<AbstractHorse*>(mule.get())) setOffspringAttributes(partner, *child);
		return mule;
	}
	auto*				 other = dynamic_cast<Horse*>(&partner);
	std::unique_ptr<Mob> foal  = _level.mobs().create(_level, _typeId);
	auto*				 child = dynamic_cast<Horse*>(foal.get());
	if (!other || !child) return foal;
	int roll	= _random.nextInt(9);
	int variant = roll < 4 ? (_typeVariant & 0xFF) : roll < 8 ? (other->_typeVariant & 0xFF) : _random.nextInt(7);
	roll		= _random.nextInt(5);
	int markings = roll < 2 ? ((_typeVariant & 0xFF00) >> 8) : roll < 4 ? ((other->_typeVariant & 0xFF00) >> 8) : _random.nextInt(5);
	child->setTypeVariant((variant & 0xFF) | ((markings << 8) & 0xFF00));
	setOffspringAttributes(partner, *child);
	return foal;
}

std::shared_ptr<SpawnGroupData> Horse::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	JavaRandom& random = _level.random();
	int			variant;
	if (auto* horses = dynamic_cast<HorseGroupData*>(group.get())) {
		variant = horses->variant;
	} else {
		variant = random.nextInt(7);
		auto data		  = std::make_shared<HorseGroupData>(variant);
		data->babySpawnChance = 0.2F;
		group				  = data;
	}
	setTypeVariant((variant & 0xFF) | ((random.nextInt(5) << 8) & 0xFF00));
	return AbstractHorse::finalizeSpawn(difficulty, reason, group);
}

void Horse::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractHorse::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_TYPE_VARIANT, onlyNonDefault, _typeVariant == 0)) writeIntData(buf, DATA_TYPE_VARIANT, _typeVariant);
}

void Horse::save(Buffer& buf) const {
	AbstractHorse::save(buf);
	buf.writeVarInt(_typeVariant);
}

void Horse::load(Buffer& buf) {
	AbstractHorse::load(buf);
	_typeVariant = buf.readVarInt();
	_dirtyData	 = 0;
}

// ===================== Donkeys and mules =====================

void AbstractChestedHorse::randomizeAttributes(JavaRandom& random) {
	if (AttributeInstance* health = _attributes.getInstance(_ids.maxHealth)) health->setBaseValue(generateMaxHealth(random));
}

AbstractChestedHorse::AbstractChestedHorse(Level& level, int typeId) : AbstractHorse(level, typeId), _chest(std::make_shared<SimpleContainer>(15)) {}

AbstractChestedHorse::~AbstractChestedHorse() = default;

void AbstractChestedHorse::setChest(bool chest) {
	if (chest == _hasChest) return;
	_hasChest = chest;
	markData(DATA_CHEST);
}

void AbstractChestedHorse::dropAllDeathLoot(const Combat::DamageSource& source) {
	AbstractHorse::dropAllDeathLoot(source);
	if (!_hasChest) return;
	// Its chest and what is in it
	spawnAtLocation(ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:chest"), 1));
	for (int slot = 0; slot < _chest->size(); slot++) {
		if (!_chest->item(slot).isEmpty()) spawnAtLocation(_chest->item(slot));
		_chest->item(slot) = ItemStack{};
	}
}

void AbstractChestedHorse::save(Buffer& buf) const {
	AbstractHorse::save(buf);
	buf.writeBool(_hasChest);
	for (int slot = 0; slot < _chest->size(); slot++) {
		const ItemStack& stack = _chest->item(slot);
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.item);
		buf.writeVarInt(stack.isEmpty() ? 0 : stack.count);
		buf.writeVarInt(static_cast<int32_t>(stack.components.size()));
		buf.writeBytes(stack.components);
	}
}

void AbstractChestedHorse::load(Buffer& buf) {
	AbstractHorse::load(buf);
	setChest(buf.readBool());
	for (int slot = 0; slot < _chest->size(); slot++) {
		ItemStack stack;
		stack.item		   = buf.readVarInt();
		stack.count		   = buf.readVarInt();
		stack.components   = buf.readBytes(static_cast<size_t>(buf.readVarInt()));
		_chest->item(slot) = stack.isEmpty() ? ItemStack{} : stack;
	}
}

bool AbstractChestedHorse::mobInteract(Player& player, int hand) {
	// AbstractChestedHorse.mobInteract: a chest on a tamed adult
	const ItemStack& held = player.getStackInHand(hand);
	if (!_hasChest && isTamed() && !isBaby() && !held.isEmpty() && held.item == _level.gameData().getStaticId("minecraft:item", "minecraft:chest")) {
		setChest(true);
		playSound("minecraft:entity.donkey.chest", 1.0F, (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		return true;
	}
	bool inventory = !isBaby() && isTamed() && player.isShiftKeyDown();
	if (!hasRider() && !inventory && !player.getStackInHand(hand).isEmpty()) {
		if (isFood(player.getStackInHand(hand))) return fedFood(player, hand);
		if (!isTamed()) {
			makeMad();
			return true;
		}
	}
	return AbstractHorse::mobInteract(player, hand);
}

void AbstractChestedHorse::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractHorse::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_CHEST, onlyNonDefault, !_hasChest)) writeBoolData(buf, DATA_CHEST, _hasChest);
}

bool Horse::canWearBody(const ItemStack& stack) const {
	// Horse armor: a body item for horses
	const GameData::ItemProperties* props = _level.gameData().getItemProperties(stack.item);
	std::string						name  = _level.gameData().getStaticName("minecraft:item", stack.item);
	return props && props->equipmentSlot == GameData::EquipmentSlot::Body && name.find("horse_armor") != std::string::npos;
}

bool Donkey::canMate(Animal& other) {
	if (&other == this) return false;
	auto* horse = dynamic_cast<AbstractHorse*>(&other);
	if (!horse || !(dynamic_cast<Donkey*>(&other) || dynamic_cast<Horse*>(&other))) return false;
	return canParent() && horse->canParent();
}

std::unique_ptr<Mob> Donkey::getBreedOffspring(Animal& partner) {
	const char*			 type  = dynamic_cast<Horse*>(&partner) ? "minecraft:mule" : "minecraft:donkey";
	std::unique_ptr<Mob> child = _level.mobs().create(_level, _level.gameData().getStaticId("minecraft:entity_type", type));
	if (auto* horse = dynamic_cast<AbstractHorse*>(child.get())) setOffspringAttributes(partner, *horse);
	return child;
}

// ===================== Undead horses =====================

void UndeadHorse::addBehaviourGoals() { _goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this)); }

void UndeadHorse::randomizeAttributes(JavaRandom& random) {
	if (AttributeInstance* jump = _attributes.getInstance(_ids.jumpStrength)) jump->setBaseValue(generateJumpStrength(random));
}

void UndeadHorse::customServerAiStep() {
	AbstractHorse::customServerAiStep();
	if (!_trap) return;
	// SkeletonHorse.aiStep: a trap that waited 15 minutes goes away
	if (++_trapTime >= 18000) return discard();
	// SkeletonTrapGoal: a player (not creative, not spectator) within 10 blocks sets it off
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || player->isCreative() || player->isSpectator()) continue;
		if ((player->position() - _position).lengthSqr() < 100.0) return triggerTrap(*player);
	}
}

void UndeadHorse::triggerTrap(Player&) {
	_trap = false;
	setTamed(true);
	setAge(0);
	auto bolt = std::make_unique<LightningBolt>(_level);
	bolt->snapTo(_position, 0.0F, 0.0F);
	bolt->setVisualOnly(true);
	_level.entities().add(std::move(bolt));
	const GameData& data	 = _level.gameData();
	int				horse	 = data.getStaticId("minecraft:entity_type", "minecraft:skeleton_horse");
	int				skeleton = data.getStaticId("minecraft:entity_type", "minecraft:skeleton");
	DifficultyInstance difficulty = _level.getCurrentDifficultyAt(blockPosition());
	// The skeleton of a horse: an iron helmet and an enchanted bow (createSkeleton / enchant)
	auto rider = [&](const Vec3& at, Mob* horse) {
		std::unique_ptr<Mob> mob = _level.mobs().create(_level, skeleton);
		if (!mob) return;
		mob->snapTo(at, _yRot, 0.0F);
		mob->finalizeSpawn(difficulty, static_cast<int>(MobRegistry::SpawnReason::Triggered));
		mob->setPersistenceRequired();
		mob->setItemSlot(EquipmentSlot::Head, ItemStack(data.getStaticId("minecraft:item", "minecraft:iron_helmet"), 1));
		ItemStack bow(data.getStaticId("minecraft:item", "minecraft:bow"), 1);
		// SkeletonTrapGoal.disenchant/enchant: EnchantmentHelper.enchantItem from the mob spawn equipment enchantments
		int cost = static_cast<int>(5.0F + difficulty.getSpecialMultiplier() * _random.nextInt(18));
		Enchantments::set(data, bow, Enchantments::selectEnchantment(data, _random, bow, cost, Enchantments::tagEntries(data, "minecraft:on_mob_spawn_equipment")));
		mob->setItemSlot(EquipmentSlot::MainHand, bow);
		Entity* added = _level.addFreshEntity(std::move(mob));
		if (added && horse) horse->addEntityPassenger(*added); // createSkeleton: it rides the horse
	};
	rider(_position, this);
	for (int i = 0; i < 3; i++) {
		std::unique_ptr<Mob> mob = _level.mobs().create(_level, horse);
		if (!mob) continue;
		Vec3 at = _position + Vec3{_random.nextGaussian() * 0.5, 0.0, _random.nextGaussian() * 0.5};
		mob->snapTo(at, _yRot, 0.0F);
		mob->finalizeSpawn(difficulty, static_cast<int>(MobRegistry::SpawnReason::Triggered));
		if (auto* undead = dynamic_cast<UndeadHorse*>(mob.get())) undead->setTamed(true);
		mob->setPersistenceRequired();
		Mob* horseAdded = static_cast<Mob*>(_level.addFreshEntity(std::move(mob)));
		rider(at, horseAdded);
	}
}

void UndeadHorse::save(Buffer& buf) const {
	AbstractHorse::save(buf);
	buf.writeBool(_trap);
	buf.writeVarInt(_trapTime);
}

void UndeadHorse::load(Buffer& buf) {
	AbstractHorse::load(buf);
	if (buf.remaining() == 0) return; // Saved before traps existed
	_trap	  = buf.readBool();
	_trapTime = buf.readVarInt();
}

bool UndeadHorse::mobInteract(Player& player, int hand) {
	if (!isTamed()) return false;
	return AbstractHorse::mobInteract(player, hand);
}
