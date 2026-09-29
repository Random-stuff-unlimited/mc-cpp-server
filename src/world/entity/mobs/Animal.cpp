#include "world/entity/mobs/Animal.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

#include <limits>

namespace {
	constexpr int SAVE_VERSION = 1;
	constexpr int EVENT_EAT_GRASS = 10; // EntityEvent: the sheep's head goes down

	// AgeableMob.AgeableMobGroupData
	struct AgeableMobGroupData : SpawnGroupData {
		int	  groupSize = 0;
		bool  shouldSpawnBaby;
		float babySpawnChance;
		AgeableMobGroupData(bool baby, float chance) : shouldSpawnBaby(baby), babySpawnChance(chance) {}
	};
} // namespace

// ===================== AgeableMob =====================

void AgeableMob::setAge(int age) {
	int old = _age;
	_age	= age;
	if ((old < 0 && age >= 0) || (old >= 0 && age < 0)) {
		markData(DATA_BABY);
		ageBoundaryReached();
	}
}

void AgeableMob::ageBoundaryReached() {
	// refreshDimensions: LivingEntity.getAgeScale, half the size as a baby
	if (isBaby()) {
		setDimensions(_type.width * 0.5F, _type.height * 0.5F, babyEyeHeight());
	} else {
		setDimensions(_type.width, _type.height, -1.0F);
	}
}

void AgeableMob::ageUp(int seconds, bool forced) {
	int old = _age;
	int age = old + seconds * 20;
	if (age > 0) age = 0;
	setAge(age);
	if (forced) {
		_forcedAge += age - old;
		if (_forcedAgeTimer == 0) _forcedAgeTimer = 40;
	}
	if (_age == 0) setAge(_forcedAge);
}

std::shared_ptr<SpawnGroupData> AgeableMob::finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) {
	if (!group) group = std::make_shared<AgeableMobGroupData>(true, babySpawnChance());
	if (auto* ageable = dynamic_cast<AgeableMobGroupData*>(group.get())) {
		if (ageable->shouldSpawnBaby && ageable->groupSize > 0 && _level.random().nextFloat() <= ageable->babySpawnChance) setAge(BABY_START_AGE);
		ageable->groupSize++;
	}
	return Mob::finalizeSpawn(difficulty, reason, group);
}

void AgeableMob::aiStep() {
	Mob::aiStep();
	if (isAlive()) {
		if (_age < 0) {
			setAge(_age + 1);
		} else if (_age > 0) {
			setAge(_age - 1);
		}
	}
}

void AgeableMob::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_BABY, onlyNonDefault, _age >= 0)) writeBoolData(buf, DATA_BABY, _age < 0);
}

void AgeableMob::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_age);
	buf.writeInt(_forcedAge);
}

void AgeableMob::load(Buffer& buf) {
	Mob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown ageable mob data version");
	setAge(buf.readInt());
	_forcedAge = buf.readInt();
	_dirtyData = 0;
}

// ===================== Animal =====================

Animal::Animal(Level& level, int typeId) : AgeableMob(level, typeId) {
	setPathfindingMalus(PathType::DangerFire, 16.0F);
	setPathfindingMalus(PathType::DamageFire, -1.0F);
}

std::string Animal::foodTag() const {
	if (_type.is("AbstractCow")) return "minecraft:cow_food";
	std::string name = _level.gameData().getStaticName("minecraft:entity_type", _typeId);
	return name + "_food";
}

bool Animal::isFood(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", foodTag(), stack.item);
}

void Animal::customServerAiStep() {
	if (_age != 0) _inLove = 0;
	AgeableMob::customServerAiStep();
}

void Animal::aiStep() {
	AgeableMob::aiStep();
	if (_age != 0) _inLove = 0;
	if (_inLove > 0) {
		_inLove--;
		// The heart particles are the client's; their random numbers are drawn all the same
		if (_inLove % 10 == 0) {
			_random.nextGaussian();
			_random.nextGaussian();
			_random.nextGaussian();
		}
	}
}

bool Animal::hurtServer(const Combat::DamageSource& source, float amount) {
	// Animal.actuallyHurt: hurt, it isn't in love anymore
	bool hurt = AgeableMob::hurtServer(source, amount);
	if (hurt) resetLove();
	return hurt;
}

float Animal::getWalkTargetValue(const BlockPos& pos) {
	int below = _level.getBlockState(pos.below());
	if (_level.gameData().getStaticName("minecraft:block", _level.blocks().blockOf(below)) == "minecraft:grass_block") return 10.0F;
	return _level.getPathfindingCostFromLightLevels(pos);
}

void Animal::usePlayerItem(Player& player, int hand) {
	if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
}

bool Animal::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	if (isFood(stack)) {
		int age = _age;
		if (age == 0 && canFallInLove()) {
			usePlayerItem(player, hand);
			setInLove(&player);
			playEatingSound();
			return true;
		}
		if (isBaby()) {
			usePlayerItem(player, hand);
			ageUp(getSpeedUpSecondsWhenFeeding(-age), true);
			playEatingSound();
			return true;
		}
	}
	return AgeableMob::mobInteract(player, hand);
}

void Animal::setInLove(Player* cause) {
	_inLove = 600;
	if (cause) _loveCause = EntityRef(*cause);
	broadcastEntityEvent(EVENT_IN_LOVE_HEARTS);
}

bool Animal::canMate(Animal& other) {
	if (&other == this) return false;
	// The same class: the same entity type here (a class per type)
	return other.typeId() == _typeId && isInLove() && other.isInLove();
}

std::unique_ptr<Mob> Animal::getBreedOffspring(Animal&) { return _level.mobs().create(_level, _typeId); }

void Animal::spawnChildFromBreeding(Animal& partner) {
	std::unique_ptr<Mob> child = getBreedOffspring(partner);
	if (!child) return;
	child->setBaby(true);
	child->snapTo(_position, 0.0F, 0.0F);
	// finalizeSpawnChildFromBreeding (the statistics and advancement of the love cause aren't ported, nor the
	// experience orbs)
	setAge(PARENT_AGE_AFTER_BREEDING);
	partner.setAge(PARENT_AGE_AFTER_BREEDING);
	resetLove();
	partner.resetLove();
	broadcastEntityEvent(EVENT_IN_LOVE_HEARTS);
	_random.nextInt(7); // The orb's experience (1 to 7)
	_level.addFreshEntity(std::move(child));
}

void Animal::save(Buffer& buf) const {
	AgeableMob::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeInt(_inLove);
}

void Animal::load(Buffer& buf) {
	AgeableMob::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown animal data version");
	_inLove = buf.readInt();
}

// ===================== BreedGoal =====================

BreedGoal::BreedGoal(Animal& animal, double speed) : _animal(animal), _speedModifier(speed) { setFlags({Flag::Move, Flag::Look}); }

Animal* BreedGoal::partner() {
	Actor* actor = _partner.isSet() ? _animal.level().actorByRef(_partner) : nullptr;
	return actor ? dynamic_cast<Animal*>(actor->asEntity()) : nullptr;
}

// PARTNER_TARGETING: non combat, 8 blocks, no line of sight needed
Animal* BreedGoal::getFreePartner() {
	TargetingConditions conditions = TargetingConditions::forNonCombat().withRange(8.0).ignoreLineOfSight();
	Animal*				best	   = nullptr;
	double				bestDist   = std::numeric_limits<double>::max();
	for (Entity* entity : _animal.level().entities().entitiesIn(_animal.boundingBox().inflate(8.0, 8.0, 8.0))) {
		auto* other = dynamic_cast<Animal*>(entity);
		if (!other || other->typeId() != _animal.typeId() || !conditions.test(_animal.level(), &_animal, *other)) continue;
		double d = _animal.distanceToSqr(*other);
		if (_animal.canMate(*other) && !other->isPanicking() && d < bestDist) {
			best	 = other;
			bestDist = d;
		}
	}
	return best;
}

bool BreedGoal::canUse() {
	if (!_animal.isInLove()) return false;
	Animal* found = getFreePartner();
	if (found) {
		_partner = EntityRef(*found);
	} else {
		_partner.clear();
	}
	return found != nullptr;
}

bool BreedGoal::canContinueToUse() {
	Animal* other = partner();
	return other && other->isAlive() && other->isInLove() && _loveTime < 60 && !other->isPanicking();
}

void BreedGoal::stop() {
	_partner.clear();
	_loveTime = 0;
}

void BreedGoal::tick() {
	Animal* other = partner();
	if (!other) return;
	_animal.lookControl().setLookAt(*other, 10.0F, static_cast<float>(_animal.maxHeadXRot()));
	_animal.navigation().moveTo(*other, _speedModifier);
	_loveTime++;
	if (_loveTime >= adjustedTickDelay(60) && _animal.distanceToSqr(*other) < 9.0) _animal.spawnChildFromBreeding(*other);
}

// ===================== FollowParentGoal =====================

bool FollowParentGoal::canUse() {
	if (_animal.getAge() >= 0) return false;
	Animal* parent	 = nullptr;
	double	bestDist = std::numeric_limits<double>::max();
	for (Entity* entity : _animal.level().entities().entitiesIn(_animal.boundingBox().inflate(8.0, 4.0, 8.0))) {
		auto* other = dynamic_cast<Animal*>(entity);
		if (!other || other->typeId() != _animal.typeId() || other->getAge() < 0) continue;
		double d = _animal.distanceToSqr(*other);
		if (!(d > bestDist)) {
			bestDist = d;
			parent	 = other;
		}
	}
	if (!parent || bestDist < 9.0) return false;
	_parent = EntityRef(*parent);
	return true;
}

bool FollowParentGoal::canContinueToUse() {
	if (_animal.getAge() >= 0) return false;
	Actor* parent = _parent.isSet() ? _animal.level().actorByRef(_parent) : nullptr;
	if (!parent || !parent->isAlive()) return false;
	double d = _animal.distanceToSqr(*parent);
	return !(d < 9.0) && !(d > 256.0);
}

void FollowParentGoal::tick() {
	if (--_timeToRecalcPath > 0) return;
	_timeToRecalcPath = adjustedTickDelay(10);
	if (Actor* parent = _animal.level().actorByRef(_parent)) _animal.navigation().moveTo(*parent, _speedModifier);
}

// ===================== EatBlockGoal =====================

EatBlockGoal::EatBlockGoal(Mob& mob) : _mob(mob) { setFlags({Flag::Move, Flag::Look, Flag::Jump}); }

namespace {
	bool edibleForSheep(Level& level, int state) {
		return level.gameData().isInTag("minecraft:block", "minecraft:edible_for_sheep", level.blocks().blockOf(state));
	}
	bool isGrassBlock(Level& level, int state) {
		return level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state)) == "minecraft:grass_block";
	}
} // namespace

bool EatBlockGoal::canUse() {
	if (_mob.random().nextInt(adjustedTickDelay(_mob.isBaby() ? 50 : 1000)) != 0) return false;
	BlockPos pos = _mob.blockPosition();
	if (edibleForSheep(_mob.level(), _mob.level().getBlockState(pos))) return true;
	return isGrassBlock(_mob.level(), _mob.level().getBlockState(pos.below()));
}

void EatBlockGoal::start() {
	_eatAnimationTick = adjustedTickDelay(40);
	Buffer event;
	event.writeInt(_mob.id());
	event.writeByte(EVENT_EAT_GRASS);
	_mob.level().entities().broadcast(_mob, PacketId::Play::Clientbound::ENTITY_EVENT, event);
	_mob.navigation().stop();
}

void EatBlockGoal::tick() {
	_eatAnimationTick = std::max(0, _eatAnimationTick - 1);
	if (_eatAnimationTick != adjustedTickDelay(4)) return;
	Level&	 level = _mob.level();
	BlockPos pos   = _mob.blockPosition();
	// The mobGriefing game rule isn't ported: always on
	if (edibleForSheep(level, level.getBlockState(pos))) {
		level.destroyBlock(pos, false);
		_mob.ate();
	} else if (isGrassBlock(level, level.getBlockState(pos.below()))) {
		BlockPos below = pos.below();
		level.levelEvent(nullptr, 2001, below, level.getBlockState(below)); // The grass block's break particles
		level.setBlock(below, level.gameData().getDefaultBlockState("minecraft:dirt"), Level::UPDATE_CLIENTS);
		_mob.ate();
	}
}
