#include "world/entity/mobs/Camelids.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Wolf.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION		= 1;
	constexpr int SERIALIZER_LONG	= 2;
	constexpr int LLAMA_VARIANTS	= 4; // creamy, white, brown, gray

	bool isItemIn(Level& level, const ItemStack& stack, const char* tag) {
		return !stack.isEmpty() && level.gameData().isInTag("minecraft:item", tag, stack.item);
	}

	// Llama.LlamaHurtByTargetGoal: spits once at what hurt it, then lets it be
	class LlamaHurtByTargetGoal : public HurtByTargetGoal {
	  public:
		explicit LlamaHurtByTargetGoal(Llama& llama) : HurtByTargetGoal(llama), _llama(llama) {}
		bool canContinueToUse() override {
			if (_llama.didSpit()) {
				_llama.setDidSpit(false);
				return false;
			}
			return HurtByTargetGoal::canContinueToUse();
		}

	  private:
		Llama& _llama;
	};

	// Llama.LlamaAttackWolfGoal: spits at the wild wolves around, once
	class LlamaAttackWolfGoal : public NearestAttackableTargetGoal {
	  public:
		explicit LlamaAttackWolfGoal(Llama& llama)
			: NearestAttackableTargetGoal(llama, "Wolf", 16, false, true,
										  [](Actor& actor, Level&) {
											  auto* wolf = dynamic_cast<Wolf*>(&actor);
											  return wolf && !wolf->isTame();
										  }),
			  _llama(llama) {}
		double getFollowDistance() override { return NearestAttackableTargetGoal::getFollowDistance() * 0.25; }

	  private:
		Llama& _llama;
	};

	// Camel's random sitting (CamelAi's RandomSitting): from time to time, on the ground, it sits down for a while, and
	// stands up when it has to go somewhere
	class CamelSitGoal : public Goal {
	  public:
		explicit CamelSitGoal(Camel& camel) : _camel(camel) { setFlags({Flag::Move, Flag::Jump}); }
		bool canUse() override {
			return !_camel.isCamelSitting() && _camel.onGround() && !_camel.isInWaterNow() && !_camel.hasRider() && _camel.poseTime() >= 160 &&
				   !_camel.isPanicking() && _camel.random().nextInt(reducedTickDelay(240)) == 0;
		}
		bool canContinueToUse() override { return _camel.isCamelSitting() && !_camel.hasRider() && !_camel.isInWaterNow() && _sitTicks > 0; }
		void start() override {
			_camel.navigation().stop();
			_camel.sitDown();
			_sitTicks = 200 + _camel.random().nextInt(400);
		}
		void tick() override { _sitTicks--; }
		void stop() override {
			if (_camel.isCamelSitting()) _camel.standUp();
		}

	  private:
		Camel& _camel;
		int	   _sitTicks = 0;
	};
} // namespace

// ===================== Llama =====================

void Llama::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<RunAroundLikeCrazyGoal>(*this, 1.2));
	_goalSelector.addGoal(3, std::make_unique<RangedAttackGoal>(*this, 1.25, 40, 20.0F));
	_goalSelector.addGoal(3, std::make_unique<PanicGoal>(*this, 1.2));
	_goalSelector.addGoal(4, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(5, std::make_unique<TemptGoal>(
								 *this, 1.25, [this](const ItemStack& stack) { return isItemIn(_level, stack, "minecraft:llama_tempt_items"); }, false));
	_goalSelector.addGoal(6, std::make_unique<FollowParentGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.7));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(9, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<LlamaHurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<LlamaAttackWolfGoal>(*this));
}

void Llama::setStrength(int strength) {
	strength = std::max(1, std::min(5, strength));
	if (strength == _strength) return;
	_strength = strength;
	markData(DATA_STRENGTH);
}

void Llama::setVariant(int variant) {
	variant = ((variant % LLAMA_VARIANTS) + LLAMA_VARIANTS) % LLAMA_VARIANTS;
	if (variant == _variant) return;
	_variant = variant;
	markData(DATA_VARIANT);
}

bool Llama::canWearBody(const ItemStack& stack) const { return isItemIn(_level, stack, "minecraft:wool_carpets"); }

bool Llama::isFood(const ItemStack& stack) const { return isItemIn(_level, stack, "minecraft:llama_food"); }

void Llama::randomizeAttributes(JavaRandom& random) {
	// Llama.setRandomStrength: up to 3, sometimes (4 %) up to 5
	int max = random.nextFloat() < 0.04F ? 5 : 3;
	setStrength(1 + random.nextInt(max));
}

bool Llama::handleEating(Player& player, const ItemStack& stack) {
	const GameData& data = _level.gameData();
	int				age = 0, temper = 0;
	float			heal = 0.0F;
	bool			used = false;
	if (stack.item == data.getStaticId("minecraft:item", "minecraft:wheat")) {
		age = 10, temper = 3, heal = 2.0F;
	} else if (stack.item == data.getStaticId("minecraft:item", "minecraft:hay_block")) {
		age = 90, temper = 6, heal = 10.0F;
		if (isTamed() && _age == 0 && canFallInLove()) {
			used = true;
			setInLove(&player);
		}
	}
	if (_health < maxHealth() && heal > 0.0F) {
		this->heal(heal);
		used = true;
	}
	if (isBaby() && age > 0) {
		ageUp(age);
		used = true;
	}
	if (temper > 0 && (used || !isTamed()) && _temper < maxTemper()) {
		used = true;
		modifyTemper(temper);
	}
	if (used) playSound("minecraft:entity.llama.eat", 1.0F, 1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
	return used;
}

void Llama::performRangedAttack(Actor& target, float) {
	auto   spit = std::make_unique<LlamaSpit>(_level, *this);
	AABB   box	= target.boundingBox();
	double dx	= target.position().x - _position.x;
	double dy	= target.position().y + (box.maxY - box.minY) / 3.0 - spit->position().y;
	double dz	= target.position().z - _position.z;
	double d	= std::sqrt(dx * dx + dz * dz) * 0.2F;
	spit->shoot(dx, dy + d, dz, 1.5F, 10.0F);
	_level.entities().add(std::move(spit));
	playSound("minecraft:entity.llama.spit", 1.0F, 1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
	_didSpit = true;
}

bool Llama::canMate(Animal& other) {
	auto* llama = dynamic_cast<Llama*>(&other);
	return llama && llama != this && canParent() && llama->canParent();
}

std::unique_ptr<Mob> Llama::getBreedOffspring(Animal& partner) {
	std::unique_ptr<Mob> baby  = _level.mobs().create(_level, _level.gameData().getStaticId("minecraft:entity_type", "minecraft:llama"));
	auto*				 child = dynamic_cast<Llama*>(baby.get());
	auto*				 other = dynamic_cast<Llama*>(&partner);
	if (!child || !other) return baby;
	// Llama.setOffspringAttributes: a stronger parent's strength plus a bit, one parent's color
	int strength = _random.nextInt(std::max(_strength, other->_strength)) + 1;
	if (_random.nextFloat() < 0.03F) strength++;
	child->setStrength(strength);
	child->setVariant(_random.nextBoolean() ? _variant : other->_variant);
	setOffspringAttributes(partner, *child);
	return baby;
}

std::shared_ptr<SpawnGroupData> Llama::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	// Llama.LlamaGroupData: the whole herd has the first one's color
	struct LlamaGroupData : AgeableMobGroupData {
		int variant;
		explicit LlamaGroupData(int v) : AgeableMobGroupData(true, 0.2F), variant(v) {}
	};
	int variant;
	if (auto* herd = dynamic_cast<LlamaGroupData*>(group.get())) {
		variant = herd->variant;
	} else {
		variant = _level.random().nextInt(LLAMA_VARIANTS);
		group	= std::make_shared<LlamaGroupData>(variant);
	}
	setVariant(variant);
	return AbstractChestedHorse::finalizeSpawn(difficulty, reason, group);
}

void Llama::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractChestedHorse::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_STRENGTH, onlyNonDefault, _strength == 0)) writeIntData(buf, DATA_STRENGTH, _strength);
	if (wantsData(mask, DATA_VARIANT, onlyNonDefault, _variant == 0)) writeIntData(buf, DATA_VARIANT, _variant);
}

void Llama::save(Buffer& buf) const {
	AbstractChestedHorse::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_strength);
	buf.writeInt(_variant);
}

void Llama::load(Buffer& buf) {
	AbstractChestedHorse::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown llama data version");
	setStrength(buf.readInt());
	setVariant(buf.readInt());
}

// ===================== TraderLlama =====================

void TraderLlama::registerGoals() {
	Llama::registerGoals();
	// TraderLlamaDefendWanderingTraderGoal: what hurt a wandering trader nearby
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "LivingEntity", 10, false, false, [this](Actor& actor, Level& level) {
		for (Actor* trader : actorsOfClass(level, "WanderingTrader", boundingBox().inflate(16.0))) {
			if (LivingEntity* living = trader->asLiving(); living && living->getLastHurtByMob() == &actor) return true;
		}
		return false;
	}));
}

void TraderLlama::aiStep() {
	Llama::aiStep();
	// TraderLlama.maybeDespawn: unless a player tamed it (it would follow its trader on a lead: not ported)
	if (!isTamed() && _despawnDelay > 0 && --_despawnDelay == 0) discard();
}

void TraderLlama::save(Buffer& buf) const {
	Llama::save(buf);
	buf.writeInt(_despawnDelay);
}

void TraderLlama::load(Buffer& buf) {
	Llama::load(buf);
	_despawnDelay = buf.readInt();
}

// ===================== LlamaSpit =====================

LlamaSpit::LlamaSpit(Level& level, Llama& spitter) : ThrowableProjectile(level, "minecraft:llama_spit", &spitter, {}) {
	// In front of its mouth
	float yBodyRot = spitter.yBodyRot() * static_cast<float>(M_PI / 180.0);
	float width	   = spitter.width();
	setPos({spitter.position().x - (width + 1.0F) * 0.5 * Mth::sin(yBodyRot), spitter.eyeY() - 0.1F,
			spitter.position().z + (width + 1.0F) * 0.5 * Mth::cos(yBodyRot)});
	_oldPosition = _position;
}

LlamaSpit::LlamaSpit(Level& level) : ThrowableProjectile(level, "minecraft:llama_spit", nullptr, {}) {}

void LlamaSpit::onHit(const Hit& hit) {
	if (hit.actor) {
		Actor*				 by = owner();
		Combat::DamageSource source{"minecraft:spit", by, this, std::nullopt};
		if (Player* player = hit.actor->asPlayer()) {
			Combat::damage(_level.server(), *player, 1.0F, source);
		} else {
			hit.actor->hurtServer(source, 1.0F);
		}
	}
	discard();
}

// ===================== Camel =====================

Camel::Camel(Level& level, int typeId) : AbstractHorse(level, typeId) {
	setPathfindingMalus(PathType::DangerPowderSnow, -1.0F);
	resetLastPoseChangeTick(-level.getGameTime());
}

void Camel::registerGoals() {
	// CamelAi: panic, breed, tempted by cacti, follow the adults, stroll, sit
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 4.0));
	_goalSelector.addGoal(2, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(3, std::make_unique<TemptGoal>(*this, 2.5, [this](const ItemStack& stack) { return isFood(stack); }, false));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 2.5));
	_goalSelector.addGoal(5, std::make_unique<CamelSitGoal>(*this));
	_goalSelector.addGoal(6, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 2.0));
	_goalSelector.addGoal(7, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
}

int64_t Camel::poseTime() const { return _level.getGameTime() - std::abs(_lastPoseChangeTick); }

void Camel::resetLastPoseChangeTick(int64_t tick) {
	if (tick == _lastPoseChangeTick) return;
	_lastPoseChangeTick = tick;
	markData(DATA_LAST_POSE_CHANGE_TICK);
}

void Camel::sitDown() {
	if (isCamelSitting()) return;
	playSound("minecraft:entity.camel.sit", 1.0F, 1.0F);
	setPose(POSE_SITTING);
	resetLastPoseChangeTick(-_level.getGameTime());
}

void Camel::standUp() {
	if (!isCamelSitting()) return;
	playSound("minecraft:entity.camel.stand", 1.0F, 1.0F);
	setPose(POSE_STANDING);
	resetLastPoseChangeTick(_level.getGameTime());
}

bool Camel::hurtServer(const Combat::DamageSource& source, float amount) {
	standUp(); // Hurt, it gets up (to flee)
	return AbstractHorse::hurtServer(source, amount);
}

bool Camel::isFood(const ItemStack& stack) const { return isItemIn(_level, stack, "minecraft:camel_food"); }

bool Camel::handleEating(Player& player, const ItemStack& stack) {
	if (!isFood(stack)) return false;
	bool used = false;
	if (_health < maxHealth()) {
		heal(2.0F);
		used = true;
	}
	if (isTamed() && _age == 0 && canFallInLove()) {
		setInLove(&player);
		used = true;
	}
	if (isBaby()) {
		ageUp(10);
		used = true;
	}
	if (used) playSound("minecraft:entity.camel.eat", 1.0F, 1.0F + (_random.nextFloat() - _random.nextFloat()) * 0.2F);
	return used;
}

bool Camel::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (player.isShiftKeyDown() && !isBaby()) return false;
	if (isFood(stack)) return fedFood(player, hand);
	if (isBaby()) return Animal::mobInteract(player, hand);
	// A saddle goes on without taming it
	if (!stack.isEmpty() && equipFromPlayer(player, hand, EquipmentSlot::Saddle)) return true;
	if (!hasRider()) {
		standUp();
		setTamed(true); // Riding a camel never throws its rider: it counts as tamed
		doPlayerRide(player);
	}
	return true;
}

bool Camel::canMate(Animal& other) {
	auto* camel = dynamic_cast<Camel*>(&other);
	return camel && camel != this && isInLove() && camel->isInLove();
}

std::unique_ptr<Mob> Camel::getBreedOffspring(Animal&) { return _level.mobs().create(_level, _typeId); }

std::shared_ptr<SpawnGroupData> Camel::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	group = AbstractHorse::finalizeSpawn(difficulty, reason, group);
	resetLastPoseChangeTick(_level.getGameTime() - 160); // Camel.resetLastPoseChangeTickToFullStand
	return group;
}

void Camel::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	AbstractHorse::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_DASH, onlyNonDefault, true)) writeBoolData(buf, DATA_DASH, false);
	if (wantsData(mask, DATA_LAST_POSE_CHANGE_TICK, onlyNonDefault, _lastPoseChangeTick == 0)) {
		buf.writeUByte(static_cast<uint8_t>(DATA_LAST_POSE_CHANGE_TICK));
		buf.writeVarInt(SERIALIZER_LONG);
		buf.writeVarLong(_lastPoseChangeTick);
	}
}

void Camel::save(Buffer& buf) const {
	AbstractHorse::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeLong(_lastPoseChangeTick);
}

void Camel::load(Buffer& buf) {
	AbstractHorse::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown camel data version");
	_lastPoseChangeTick = buf.readLong();
	if (isCamelSitting()) setPose(POSE_SITTING);
}
