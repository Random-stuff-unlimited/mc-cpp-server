#include "world/entity/mobs/Wolf.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Variants.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/item/ItemDamage.hpp"

#include <cmath>

namespace {
	constexpr int		  SAVE_VERSION = 1;
	constexpr int		  SERIALIZER_WOLF_VARIANT = 23, SERIALIZER_WOLF_SOUND_VARIANT = 24;
	constexpr const char* VARIANTS		 = "minecraft:wolf_variant";
	constexpr const char* SOUND_VARIANTS = "minecraft:wolf_sound_variant";

	bool isType(Level& level, Actor& actor, const char* javaClass) {
		const GameData::EntityTypeInfo* type = level.gameData().getEntityType(actor.typeId());
		return type && type->is(javaClass);
	}
	const std::string& typeName(Level& level, Actor& actor) { return level.gameData().getStaticName("minecraft:entity_type", actor.typeId()); }

	int variantCount(Level& level, const char* registry) {
		int count = 0;
		while (!Variants::name(level, registry, count).empty()) count++;
		return count;
	}

	// Wolf.WolfPackData: a pack's wolves share their coat, none is born a pup
	struct WolfPackData : SpawnGroupData {
		int variant;
		explicit WolfPackData(int v) : variant(v) {}
	};

	// BegGoal: tilts its head at a player holding a bone or meat within 8 blocks
	class BegGoal : public Goal {
	  public:
		BegGoal(Wolf& wolf, float lookDistance) : _wolf(wolf), _lookDistance(lookDistance) { setFlags({Flag::Look}); }
		bool canUse() override {
			TargetingConditions conditions = TargetingConditions::forNonCombat();
			conditions.withRange(_lookDistance);
			const Vec3& p	   = _wolf.position();
			Actor*		player = nearestPlayer(_wolf.level(), conditions, &_wolf, p.x, p.y, p.z);
			if (!player || !holdingInteresting(*player)) return false;
			_player = EntityRef(*player);
			return true;
		}
		bool canContinueToUse() override {
			Actor* player = _wolf.level().actorByRef(_player);
			if (!player || !player->isAlive()) return false;
			if (_wolf.distanceToSqr(*player) > static_cast<double>(_lookDistance) * _lookDistance) return false;
			return _lookTime > 0 && holdingInteresting(*player);
		}
		void start() override {
			_wolf.setIsInterested(true);
			_lookTime = adjustedTickDelay(40 + _wolf.random().nextInt(40));
		}
		void stop() override {
			_wolf.setIsInterested(false);
			_player.clear();
		}
		void tick() override {
			if (Actor* player = _wolf.level().actorByRef(_player)) {
				_wolf.lookControl().setLookAt(player->position().x, player->eyeY(), player->position().z, 10.0F, static_cast<float>(_wolf.maxHeadXRot()));
			}
			_lookTime--;
		}

	  private:
		Wolf&	  _wolf;
		EntityRef _player;
		float	  _lookDistance;
		int		  _lookTime = 0;
		bool	  holdingInteresting(Actor& actor) {
			 Player* player = actor.asPlayer();
			 if (!player) return false;
			 int bone = _wolf.level().gameData().getStaticId("minecraft:item", "minecraft:bone");
			 for (int hand = 0; hand < 2; hand++) {
				 const ItemStack& stack = player->getStackInHand(hand);
				 if (!stack.isEmpty() && (stack.item == bone || _wolf.isFood(stack))) return true;
			 }
			 return false;
		}
	};
} // namespace

Wolf::Wolf(Level& level, int typeId) : TamableAnimal(level, typeId), _anger(*this, 20, 39) {
	setPathfindingMalus(PathType::PowderSnow, -1.0F);
	setPathfindingMalus(PathType::DangerPowderSnow, -1.0F);
	// WolfVariants.DEFAULT (pale) and WolfSoundVariants.CLASSIC, the client's defaults
	_defaultVariant		 = std::max(0, Variants::id(level, VARIANTS, "minecraft:pale"));
	_defaultSoundVariant = std::max(0, Variants::id(level, SOUND_VARIANTS, "minecraft:classic"));
	_variant			 = _defaultVariant;
	_soundVariant		 = _defaultSoundVariant;
}

void Wolf::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<TamableAnimalPanicGoal>(*this, 1.5, "minecraft:panic_environmental_causes"));
	_goalSelector.addGoal(2, std::make_unique<SitWhenOrderedToGoal>(*this));
	// WolfAvoidEntityGoal (llamas, by their strength): llamas have no class yet
	_goalSelector.addGoal(4, std::make_unique<LeapAtTargetGoal>(*this, 0.4F));
	_goalSelector.addGoal(5, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(6, std::make_unique<FollowOwnerGoal>(*this, 1.0, 10.0F, 2.0F));
	_goalSelector.addGoal(7, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(8, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(9, std::make_unique<BegGoal>(*this, 8.0F));
	_goalSelector.addGoal(10, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(10, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<OwnerHurtByTargetGoal>(*this, true));
	_targetSelector.addGoal(2, std::make_unique<OwnerHurtByTargetGoal>(*this, false)); // OwnerHurtTargetGoal
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(3, std::move(hurtBy));
	_targetSelector.addGoal(4, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor& target, Level&) { return _anger.isAngryAt(target); }));
	// PREY_SELECTOR: sheep, rabbits, foxes
	_targetSelector.addGoal(5, std::make_unique<NonTameRandomTargetGoal>(*this, "Animal", false, [](Actor& target, Level& level) {
		const std::string& name = typeName(level, target);
		return name == "minecraft:sheep" || name == "minecraft:rabbit" || name == "minecraft:fox";
	}));
	_targetSelector.addGoal(6, std::make_unique<NonTameRandomTargetGoal>(*this, "Turtle", false, [](Actor& target, Level&) {
		LivingEntity* turtle = target.asLiving();
		return turtle && turtle->isBaby() && !turtle->isInWaterNow(); // Turtle.BABY_ON_LAND_SELECTOR
	}));
	_targetSelector.addGoal(7, std::make_unique<NearestAttackableTargetGoal>(*this, "AbstractSkeleton", false));
	// ResetUniversalAngerTargetGoal: the universalAnger game rule is off
}

// ----- Entity data -----

void Wolf::setCollarColor(int color) {
	if (color == _collarColor) return;
	_collarColor = color;
	markData(DATA_COLLAR_COLOR);
}

void Wolf::setVariant(int variant) {
	if (variant < 0 || variant == _variant) return;
	_variant = variant;
	markData(DATA_VARIANT);
}

void Wolf::setSoundVariant(int variant) {
	if (variant < 0 || variant == _soundVariant) return;
	_soundVariant = variant;
	markData(DATA_SOUND_VARIANT);
}

void Wolf::setIsInterested(bool interested) {
	if (interested == _interested) return;
	_interested = interested;
	markData(DATA_INTERESTED);
}

void Wolf::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	TamableAnimal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_INTERESTED, onlyNonDefault, !_interested)) writeBoolData(buf, DATA_INTERESTED, _interested);
	if (wantsData(mask, DATA_COLLAR_COLOR, onlyNonDefault, _collarColor == DEFAULT_COLLAR_COLOR)) writeIntData(buf, DATA_COLLAR_COLOR, _collarColor);
	if (wantsData(mask, DATA_REMAINING_ANGER_TIME, onlyNonDefault, _syncedAngerTime == 0)) writeIntData(buf, DATA_REMAINING_ANGER_TIME, _syncedAngerTime);
	if (wantsData(mask, DATA_VARIANT, onlyNonDefault, _variant == _defaultVariant)) {
		buf.writeUByte(DATA_VARIANT);
		buf.writeVarInt(SERIALIZER_WOLF_VARIANT);
		buf.writeVarInt(_variant);
	}
	if (wantsData(mask, DATA_SOUND_VARIANT, onlyNonDefault, _soundVariant == _defaultSoundVariant)) {
		buf.writeUByte(DATA_SOUND_VARIANT);
		buf.writeVarInt(SERIALIZER_WOLF_SOUND_VARIANT);
		buf.writeVarInt(_soundVariant);
	}
}

// ----- Sounds -----

std::string Wolf::variantSound(const char* name) const {
	// WolfSoundVariants.SoundSet: "entity.wolf<_suffix>.<name>", no suffix for the classic one
	std::string variant = Variants::name(_level, SOUND_VARIANTS, _soundVariant);
	variant				= variant.substr(variant.find(':') + 1);
	return "minecraft:entity.wolf" + (variant.empty() || variant == "classic" ? std::string() : "_" + variant) + "." + name;
}

std::string Wolf::ambientSoundName() {
	if (isAngry()) return variantSound("growl");
	if (_random.nextInt(3) == 0) return variantSound(isTame() && _health < 20.0F ? "whine" : "pant");
	return variantSound("ambient");
}

std::string Wolf::hurtSoundName() const { return variantSound("hurt"); }

// ----- Tick -----

void Wolf::aiStep() {
	TamableAnimal::aiStep();
	if (isRemoved()) return;
	if (_isWet && !_isShaking && navigation().isDone() && _onGround) {
		_isShaking = true;
		_shakeAnim = _shakeAnimO = 0.0F;
		broadcastEntityEvent(EVENT_START_SHAKING);
	}
	_anger.update(true);
	if (_anger.remainingTime() != _syncedAngerTime) {
		_syncedAngerTime = _anger.remainingTime();
		markData(DATA_REMAINING_ANGER_TIME);
	}
}

void Wolf::tick() {
	TamableAnimal::tick();
	if (!isAlive()) return;
	if (isInWaterOrRain()) {
		_isWet = true;
		if (_isShaking) {
			broadcastEntityEvent(EVENT_CANCEL_SHAKING);
			_isShaking = false;
			_shakeAnim = _shakeAnimO = 0.0F;
		}
	} else if (_isShaking) {
		if (_shakeAnim == 0.0F) playSound("minecraft:entity.wolf.shake", soundVolume(), (_random.nextFloat() - _random.nextFloat()) * 0.2F + 1.0F);
		_shakeAnimO = _shakeAnim;
		_shakeAnim += 0.05F;
		if (_shakeAnimO >= 2.0F) {
			_isWet = _isShaking = false;
			_shakeAnim = _shakeAnimO = 0.0F;
		}
		if (_shakeAnim > 0.4F) {
			// The splash particles are the clients'; their random draws are made here too
			int drops = static_cast<int>(Mth::sin((_shakeAnim - 0.4F) * (float)M_PI) * 7.0F);
			for (int i = 0; i < drops; i++) {
				_random.nextFloat();
				_random.nextFloat();
			}
		}
	}
}

void Wolf::die(const Combat::DamageSource& source) {
	_isWet = _isShaking = false;
	_shakeAnim = _shakeAnimO = 0.0F;
	TamableAnimal::die(source);
}

// ----- Taming, feeding, armor -----

bool Wolf::isFood(const ItemStack& stack) const { return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:wolf_food", stack.item); }

void Wolf::applyTamingSideEffects() {
	AttributeInstance* maxHealth = _attributes.getInstance(_ids.maxHealth);
	if (!maxHealth) return;
	if (isTame()) {
		maxHealth->setBaseValue(40.0);
		setHealth(40.0F);
	} else {
		maxHealth->setBaseValue(8.0);
	}
}

bool Wolf::hasWolfArmor() const {
	const ItemStack& body = getItemBySlot(EquipmentSlot::Body);
	return !body.isEmpty() && body.item == _level.gameData().getStaticId("minecraft:item", "minecraft:wolf_armor");
}

bool Wolf::hurtServer(const Combat::DamageSource& source, float amount) {
	setOrderedToSit(false);
	return TamableAnimal::hurtServer(source, amount);
}

void Wolf::actuallyHurt(const Combat::DamageSource& source, float amount, int damageType) {
	// Its armor takes the damage in its place
	if (!hasWolfArmor() || _level.gameData().isInTag("minecraft:damage_type", "minecraft:bypasses_wolf_armor", damageType)) {
		TamableAnimal::actuallyHurt(source, amount, damageType);
		return;
	}
	ItemStack& armor  = _equipment[static_cast<int>(EquipmentSlot::Body)];
	auto	   cracks = [&](const ItemStack& stack) {
		  // Crackiness.WOLF_ARMOR by the remaining durability: none, low, medium, high
		  if (stack.isEmpty()) return 3;
		  int	max		 = ItemDamage::maxDamage(_level.gameData(), stack);
		  float fraction = max > 0 ? static_cast<float>(max - ItemDamage::damage(_level.gameData(), stack)) / max : 1.0F;
		  return fraction < 0.25F ? 3 : fraction < 0.5F ? 2 : fraction < 0.95F ? 1 : 0;
	};
	int before = cracks(armor);
	ItemDamage::hurtAndBreak(_level, armor, Mth::ceil(amount), nullptr, 0);
	if (cracks(armor) != before) playSound("minecraft:item.wolf_armor.crack", 1.0F, 1.0F);
}

void Wolf::tryToTame(Player& player) {
	if (_random.nextInt(3) == 0) {
		tame(player);
		navigation().stop();
		setTarget(nullptr);
		setOrderedToSit(true);
		broadcastEntityEvent(EVENT_TAMING_SUCCEEDED);
	} else {
		broadcastEntityEvent(EVENT_TAMING_FAILED);
	}
}

bool Wolf::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	const GameData&	 data  = _level.gameData();
	auto			 consume = [&] {
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
	};
	if (isTame()) {
		if (isFood(stack) && _health < maxHealth()) {
			const GameData::ItemProperties* props	  = data.getItemProperties(stack.item);
			float							nutrition = props && props->food ? static_cast<float>(props->food->nutrition) : 1.0F;
			usePlayerItem(player, hand);
			heal(2.0F * nutrition);
			return true;
		}
		bool owned = isOwnedBy(player);
		int	 dye   = dyeColorOfItem(_level, stack);
		if (dye >= 0 && owned) {
			if (dye != _collarColor) {
				setCollarColor(dye);
				consume();
				return true;
			}
			return TamableAnimal::mobInteract(player, hand);
		}
		if (!stack.isEmpty() && stack.item == data.getStaticId("minecraft:item", "minecraft:wolf_armor") && getItemBySlot(EquipmentSlot::Body).isEmpty() && owned &&
			!isBaby()) {
			setItemSlot(EquipmentSlot::Body, stack.copyWithCount(1));
			consume();
			return true;
		}
		ItemStack& armor = _equipment[static_cast<int>(EquipmentSlot::Body)];
		if (isInSittingPose() && hasWolfArmor() && owned && ItemDamage::damage(data, armor) > 0 && !stack.isEmpty() &&
			stack.item == data.getStaticId("minecraft:item", "minecraft:armadillo_scute")) {
			player.inventory().getMutable(player.handSlot(hand)).shrink(1);
			playSound("minecraft:item.wolf_armor.repair", 1.0F, 1.0F);
			int repair = static_cast<int>(ItemDamage::maxDamage(data, armor) * 0.125F);
			ItemDamage::setDamage(data, armor, std::max(0, ItemDamage::damage(data, armor) - repair));
			return true;
		}
		bool consumed = TamableAnimal::mobInteract(player, hand);
		if (!consumed && owned) {
			setOrderedToSit(!isOrderedToSit());
			_jumping = false;
			navigation().stop();
			setTarget(nullptr);
			return true;
		}
		return consumed;
	}
	if (!stack.isEmpty() && stack.item == data.getStaticId("minecraft:item", "minecraft:bone") && !isAngry()) {
		consume();
		tryToTame(player);
		return true;
	}
	return TamableAnimal::mobInteract(player, hand);
}

bool Wolf::wantsToAttack(Actor& target, Actor& owner) {
	if (isType(_level, target, "Creeper") || isType(_level, target, "Ghast") || isType(_level, target, "ArmorStand")) return false;
	if (auto* wolf = dynamic_cast<Wolf*>(&target)) return !wolf->isTame() || wolf->getOwner() != &owner;
	if (isType(_level, target, "AbstractHorse")) return true; // Tamed horses are spared: horses have no class yet
	auto* tamable = dynamic_cast<TamableAnimal*>(&target);
	return !(tamable && tamable->isTame());
}

// ----- Spawning and breeding -----

std::shared_ptr<SpawnGroupData> Wolf::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	if (auto* pack = dynamic_cast<WolfPackData*>(group.get())) {
		setVariant(pack->variant);
	} else {
		int variant = Variants::selectToSpawn(_level, VARIANTS, blockPosition());
		if (variant >= 0) {
			setVariant(variant);
			group = std::make_shared<WolfPackData>(variant);
		}
	}
	int sounds = variantCount(_level, SOUND_VARIANTS);
	if (sounds > 0) setSoundVariant(_level.random().nextInt(sounds)); // pickRandomSoundVariant
	// WolfPackData is AgeableMobGroupData(false): no pups in a pack
	std::shared_ptr<SpawnGroupData> result = group;
	Mob::finalizeSpawn(difficulty, reason, group);
	return result;
}

bool Wolf::canMate(Animal& other) {
	if (&other == this || !isTame()) return false;
	auto* wolf = dynamic_cast<Wolf*>(&other);
	if (!wolf || !wolf->isTame() || wolf->isInSittingPose()) return false;
	return isInLove() && wolf->isInLove();
}

std::unique_ptr<Mob> Wolf::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> child = TamableAnimal::getBreedOffspring(partner);
	auto*				 pup   = dynamic_cast<Wolf*>(child.get());
	auto*				 other = dynamic_cast<Wolf*>(&partner);
	if (!pup || !other) return child;
	pup->setVariant(_random.nextBoolean() ? _variant : other->_variant);
	if (isTame()) {
		pup->setOwnerReference(ownerReference());
		pup->setTame(true, true);
		pup->setCollarColor(mixedDyeColor(_level, _collarColor, other->_collarColor));
	}
	int sounds = variantCount(_level, SOUND_VARIANTS);
	if (sounds > 0) pup->setSoundVariant(_random.nextInt(sounds));
	return child;
}

std::string Wolf::lootComponent(const std::string& component) const {
	if (component == "minecraft:wolf/variant") return Variants::name(_level, VARIANTS, _variant);
	return TamableAnimal::lootComponent(component);
}

// ----- Saving -----

void Wolf::save(Buffer& buf) const {
	TamableAnimal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeVarInt(_collarColor);
	buf.writeString(Variants::name(_level, VARIANTS, _variant));
	buf.writeString(Variants::name(_level, SOUND_VARIANTS, _soundVariant));
	_anger.save(buf);
}

void Wolf::load(Buffer& buf) {
	TamableAnimal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown wolf data version");
	_collarColor	 = buf.readVarInt();
	int variant		 = Variants::id(_level, VARIANTS, buf.readString());
	int soundVariant = Variants::id(_level, SOUND_VARIANTS, buf.readString());
	if (variant >= 0) _variant = variant;
	if (soundVariant >= 0) _soundVariant = soundVariant;
	_anger.load(buf);
	_dirtyData = 0;
}
