#include "world/entity/mobs/PolarBear.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

namespace {
	constexpr int SAVE_VERSION = 1;

	// PolarBear.PolarBearMeleeAttackGoal: stands up as the hit comes, when its target is close
	class PolarBearMeleeAttackGoal : public MeleeAttackGoal {
	  public:
		explicit PolarBearMeleeAttackGoal(PolarBear& bear) : MeleeAttackGoal(bear, 1.25, true), _bear(bear) {}
		void stop() override {
			_bear.setStanding(false);
			MeleeAttackGoal::stop();
		}

	  protected:
		void checkAndPerformAttack(Actor& target) override {
			AABB   box	 = target.boundingBox();
			double reach = (box.maxX - box.minX) + 3.0F;
			if (canPerformAttack(target)) {
				resetAttackCooldown();
				_mob.doHurtTarget(target);
				_bear.setStanding(false);
			} else if (_mob.distanceToSqr(target) < reach * reach) {
				if (isTimeToAttack()) {
					_bear.setStanding(false);
					resetAttackCooldown();
				}
				if (getTicksUntilNextAttack() <= 10) {
					_bear.setStanding(true);
					_bear.playWarningSound();
				}
			} else {
				resetAttackCooldown();
				_bear.setStanding(false);
			}
		}

	  private:
		PolarBear& _bear;
	};

	// Cubs panic at anything that hurts, adults only at fire and the like
	class PolarBearPanicGoal : public PanicGoal {
	  public:
		using PanicGoal::PanicGoal;

	  protected:
		bool shouldPanic() override {
			_panicTag = static_cast<LivingEntity&>(_mob).isBaby() ? "minecraft:panic_causes" : "minecraft:panic_environmental_causes";
			return PanicGoal::shouldPanic();
		}
	};

	// PolarBear.PolarBearHurtByTargetGoal: a hurt cub doesn't fight, it calls the adults
	class PolarBearHurtByTargetGoal : public HurtByTargetGoal {
	  public:
		explicit PolarBearHurtByTargetGoal(PolarBear& bear) : HurtByTargetGoal(bear) {}
		void start() override {
			HurtByTargetGoal::start();
			if (static_cast<LivingEntity&>(_mob).isBaby()) {
				alertOthers();
				stop();
			}
		}

	  protected:
		void alertOther(Mob& other, Actor& target) override {
			if (dynamic_cast<PolarBear*>(&other) && !other.isBaby()) HurtByTargetGoal::alertOther(other, target);
		}
	};

	// PolarBear.PolarBearAttackPlayersGoal: an adult with a cub within 8 blocks attacks the players within half its range
	class PolarBearAttackPlayersGoal : public NearestAttackableTargetGoal {
	  public:
		explicit PolarBearAttackPlayersGoal(PolarBear& bear) : NearestAttackableTargetGoal(bear, "Player", 20, true, true) {}
		bool canUse() override {
			if (static_cast<LivingEntity&>(_mob).isBaby()) return false;
			if (!NearestAttackableTargetGoal::canUse()) return false;
			bool cub = false;
			_mob.level().entities().forEachIn(_mob.boundingBox().inflate(8.0, 4.0, 8.0), [&](Entity& entity) {
				auto* bear = dynamic_cast<PolarBear*>(&entity);
				cub |= bear && !bear->isRemoved() && bear->isBaby();
			});
			return cub;
		}

	  protected:
		double getFollowDistance() override { return NearestAttackableTargetGoal::getFollowDistance() * 0.5; }
	};
} // namespace

void PolarBear::registerGoals() {
	_goalSelector.addGoal(0, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PolarBearMeleeAttackGoal>(*this));
	_goalSelector.addGoal(1, std::make_unique<PolarBearPanicGoal>(*this, 2.0));
	_goalSelector.addGoal(4, std::make_unique<FollowParentGoal>(*this, 1.25));
	_goalSelector.addGoal(5, std::make_unique<RandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 6.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<PolarBearHurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<PolarBearAttackPlayersGoal>(*this));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", 10, true, false,
																		   [this](Actor& target, Level&) { return _anger.isAngryAt(target); }));
	_targetSelector.addGoal(4, std::make_unique<NearestAttackableTargetGoal>(*this, "Fox", 10, true, true));
	// ResetUniversalAngerTargetGoal: the universalAnger game rule is off
}

void PolarBear::setStanding(bool standing) {
	if (standing == _standing) return;
	_standing = standing;
	markData(DATA_STANDING);
}

void PolarBear::playWarningSound() {
	if (_warningSoundTicks > 0) return;
	playSound("minecraft:entity.polar_bear.warning", soundVolume(), voicePitch()); // makeSound
	_warningSoundTicks = 40;
}

void PolarBear::tick() {
	Animal::tick();
	if (_warningSoundTicks > 0) _warningSoundTicks--;
	if (!isRemoved()) _anger.update(true);
}

void PolarBear::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Animal::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_STANDING, onlyNonDefault, !_standing)) writeBoolData(buf, DATA_STANDING, _standing);
}

void PolarBear::save(Buffer& buf) const {
	Animal::save(buf);
	buf.writeUByte(SAVE_VERSION);
	_anger.save(buf);
}

void PolarBear::load(Buffer& buf) {
	Animal::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown polar bear data version");
	_anger.load(buf);
	_dirtyData = 0;
}
