#include "world/entity/mobs/Hoglin.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr int SAVE_VERSION = 1;

	// HoglinBase.throwTarget
	void throwTarget(Mob& attacker, Actor& target) {
		double knockback  = attacker.getAttributeValue(attacker.attributeIds().attackKnockback);
		double resistance = 0.0;
		if (LivingEntity* living = target.asLiving()) resistance = living->getAttributeValue(living->attributeIds().knockbackResistance);
		double strength = knockback - resistance;
		if (strength <= 0.0) return;
		JavaRandom& random = attacker.random();
		double		dx	   = target.position().x - attacker.position().x;
		double		dz	   = target.position().z - attacker.position().z;
		float		angle  = static_cast<float>(random.nextInt(21) - 10) * (static_cast<float>(M_PI) / 180.0F);
		double		scale  = strength * (random.nextFloat() * 0.5F + 0.2F);
		Vec3		flat   = Vec3{dx, 0.0, dz}.normalize().scale(scale);
		// Vec3.yRot
		float cos = std::cos(angle), sin = std::sin(angle);
		Vec3  push{flat.x * cos + flat.z * sin, strength * random.nextFloat() * 0.5, flat.z * cos - flat.x * sin};
		target.pushMotion(push);
	}

	// A hoglin's hunting: the players (not creative), and piglins it isn't outnumbered by in vanilla (here: always)
	bool isHuntable(Actor& actor, Level&) { return notCreativeOrSpectator(actor); }
} // namespace

bool hoglinHurtAndThrowTarget(Mob& attacker, Actor& target) {
	float damage = static_cast<float>(attacker.getAttributeValue(attacker.attributeIds().attackDamage));
	float dealt	 = !attacker.isBaby() && static_cast<int>(damage) > 0 ? damage / 2.0F + attacker.random().nextInt(static_cast<int>(damage)) : damage;
	Combat::DamageSource source{"minecraft:mob_attack", &attacker, nullptr, std::nullopt};
	bool hurt = target.asPlayer() ? Combat::damage(attacker.level().server(), *target.asPlayer(), dealt, source) : target.hurtServer(source, dealt);
	if (hurt) {
		attacker.setLastHurtMob(&target);
		if (!attacker.isBaby()) throwTarget(attacker, target);
	}
	return hurt;
}

// ===================== Hoglin =====================

Hoglin::Hoglin(Level& level, int typeId) : Animal(level, typeId) { _xpReward = 5; }

void Hoglin::registerGoals() {
	// HoglinAi: fight (adults), avoid (babies and the outnumbered), breed, follow the adults, idle
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PanicGoal>(*this, 1.4)); // Babies only: adults turn on what hurt them
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(3, std::make_unique<BreedGoal>(*this, 1.0));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 0.6));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.4));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));

	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers(); // HoglinAi.broadcastAttackTarget: the herd joins in
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false, isHuntable));
}

bool Hoglin::isConverting() const { return !_level.dimensionType().piglinSafe && !_immuneToZombification && !isNoAi(); }

void Hoglin::setImmuneToZombification(bool immune) {
	if (immune == _immuneToZombification) return;
	_immuneToZombification = immune;
	markData(DATA_IMMUNE_TO_ZOMBIFICATION);
}

bool Hoglin::doHurtTarget(Actor& target) {
	if (isBaby()) return false; // Babies don't fight (their attack target is dropped by the Brain)
	broadcastEntityEvent(EVENT_ATTACK);
	playSound("minecraft:entity.hoglin.attack", 1.0F, 1.0F);
	return hoglinHurtAndThrowTarget(*this, target);
}

void Hoglin::customServerAiStep() {
	// Babies don't keep a target: they panic instead
	if (isBaby() && getTarget()) setTarget(nullptr);
	if (isConverting()) {
		_timeInOverworld++;
		if (_timeInOverworld > CONVERSION_TIME) {
			playSound("minecraft:entity.hoglin.converted_to_zombified", 1.0F, 1.0F);
			bool baby = isBaby();
			convertTo(_level.gameData().getStaticId("minecraft:entity_type", "minecraft:zoglin"), true, false, [baby](Mob& zoglin) {
				zoglin.setBaby(baby);
				zoglin.addEffect({MobEffects::Nausea, 200, 0});
			});
			return;
		}
	} else {
		_timeInOverworld = 0;
	}
	Animal::customServerAiStep();
}

void Hoglin::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_IMMUNE_TO_ZOMBIFICATION, onlyNonDefault, !_immuneToZombification)) {
		writeBoolData(buf, DATA_IMMUNE_TO_ZOMBIFICATION, _immuneToZombification);
	}
}

void Hoglin::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_immuneToZombification);
	buf.writeInt(_timeInOverworld);
}

void Hoglin::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown hoglin data version");
	setImmuneToZombification(buf.readBool());
	_timeInOverworld = buf.readInt();
}

// ===================== Zoglin =====================

void Zoglin::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.4));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	// Zoglin.isTargetable: any living thing but zoglins and creepers, the nearest first
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "LivingEntity", 10, true, false, [this](Actor& actor, Level&) {
		return canAttackType(actor.typeId()) && notCreativeOrSpectator(actor);
	}));
}

bool Zoglin::canAttackType(int typeId) const {
	const GameData::EntityTypeInfo* type = _level.gameData().getEntityType(typeId);
	return type && !type->is("Zoglin") && !type->is("Creeper");
}

void Zoglin::setBaby(bool baby) {
	if (_baby != baby) {
		_baby = baby;
		markData(DATA_BABY);
	}
	if (AttributeInstance* damage = _attributes.getInstance(_ids.attackDamage)) {
		// Zoglin.setBaby: babies deal 0.5
		damage->setBaseValue(baby ? 0.5 : 6.0);
	}
	if (baby) {
		setDimensions(_type.width * 0.5F, _type.height * 0.5F, -1.0F);
	} else {
		setDimensions(_type.width, _type.height, -1.0F);
	}
}

bool Zoglin::doHurtTarget(Actor& target) {
	broadcastEntityEvent(Hoglin::EVENT_ATTACK);
	playSound("minecraft:entity.zoglin.attack", 1.0F, 1.0F);
	return hoglinHurtAndThrowTarget(*this, target);
}

void Zoglin::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_BABY, onlyNonDefault, !_baby)) writeBoolData(buf, DATA_BABY, _baby);
}

void Zoglin::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_baby);
}

void Zoglin::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown zoglin data version");
	setBaby(buf.readBool());
}

// ===================== Giant =====================

float Giant::getWalkTargetValue(const BlockPos& pos) { return _level.getPathfindingCostFromLightLevels(pos); }
