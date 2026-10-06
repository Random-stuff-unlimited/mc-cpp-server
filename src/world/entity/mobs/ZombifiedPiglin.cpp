#include "world/entity/mobs/ZombifiedPiglin.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <algorithm>

namespace {
	constexpr int		  SAVE_VERSION = 2;
	constexpr const char* SPEED_MODIFIER_ATTACKING = "minecraft:attacking";
} // namespace

ZombifiedPiglin::ZombifiedPiglin(Level& level, int typeId) : Zombie(level, typeId), _anger(*this, 20, 39) { setPathfindingMalus(PathType::Lava, 8.0F); }

void ZombifiedPiglin::populateDefaultEquipmentSlots(DifficultyInstance&) {
	setItemSlot(EquipmentSlot::MainHand, ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:golden_sword"), 1));
}

void ZombifiedPiglin::addBehaviourGoals() {
	_goalSelector.addGoal(2, std::make_unique<ZombieAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor& target, Level&) { return _anger.isAngryAt(target); }));
	// ResetUniversalAngerTargetGoal: the universalAnger game rule is off
}

void ZombifiedPiglin::customServerAiStep() {
	AttributeInstance* speed = _attributes.getInstance(_ids.movementSpeed);
	if (isAngry()) {
		if (!isBaby() && speed && !speed->hasModifier(SPEED_MODIFIER_ATTACKING)) {
			speed->addModifier({SPEED_MODIFIER_ATTACKING, 0.05, AttributeModifier::Operation::AddValue}, false);
		}
		// maybePlayFirstAngerSound
		if (_playFirstAngerSoundIn > 0 && --_playFirstAngerSoundIn == 0) {
			playSound("minecraft:entity.zombified_piglin.angry", soundVolume() * 2.0F, voicePitch() * 1.8F);
		}
	} else if (speed && speed->hasModifier(SPEED_MODIFIER_ATTACKING)) {
		speed->removeModifier(SPEED_MODIFIER_ATTACKING);
	}
	_anger.update(true);
	if (Actor* target = getTarget()) {
		// maybeAlertOthers
		if (_ticksUntilNextAlert > 0) {
			_ticksUntilNextAlert--;
		} else {
			if (sensing().hasLineOfSight(*target)) alertOthers();
			_ticksUntilNextAlert = _random.nextInt(41) + 80; // ALERT_INTERVAL: 4 to 6 seconds
		}
	}
	Zombie::customServerAiStep();
}

void ZombifiedPiglin::alertOthers() {
	Actor* target = getTarget();
	if (!target) return;
	double follow = getAttributeValue(_ids.followRange);
	AABB   area	  = AABB{_position.x, _position.y, _position.z, _position.x + 1.0, _position.y + 1.0, _position.z + 1.0}.inflate(follow, 10.0, follow);
	std::vector<ZombifiedPiglin*> others;
	_level.entities().forEachIn(area, [&](Entity& entity) {
		auto* other = dynamic_cast<ZombifiedPiglin*>(&entity);
		if (other && other != this && !other->isRemoved() && !other->getTarget()) others.push_back(other);
	});
	for (ZombifiedPiglin* other : others) other->setTarget(target);
}

void ZombifiedPiglin::setTarget(Actor* target) {
	if (!getTarget() && target) {
		_playFirstAngerSoundIn = _random.nextInt(21);	   // FIRST_ANGER_SOUND_DELAY: 0 to 1 second
		_ticksUntilNextAlert   = _random.nextInt(41) + 80; // ALERT_INTERVAL
	}
	Zombie::setTarget(target);
}

void ZombifiedPiglin::randomizeReinforcementsChance() {
	if (AttributeInstance* chance = _attributes.getInstance(_ids.spawnReinforcements)) chance->setBaseValue(0.0);
}

bool ZombifiedPiglin::isPreventingPlayerRest(Player& player) const { return _anger.isAngryAt(player); }

std::string ZombifiedPiglin::ambientSoundName() { return isAngry() ? "minecraft:entity.zombified_piglin.angry" : "minecraft:entity.zombified_piglin.ambient"; }

void ZombifiedPiglin::save(Buffer& buf) const {
	Zombie::save(buf);
	buf.writeUByte(SAVE_VERSION);
	_anger.save(buf);
}

void ZombifiedPiglin::load(Buffer& buf) {
	Zombie::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown zombified piglin data version");
	_anger.load(buf);
}
