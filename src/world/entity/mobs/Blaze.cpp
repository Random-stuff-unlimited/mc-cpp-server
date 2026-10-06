#include "world/entity/mobs/Blaze.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/Fireballs.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/entity/ai/RandomPos.hpp"

#include <cmath>

namespace {
	constexpr int LEVEL_EVENT_BLAZE_SHOOT = 1018;

	// RandomSource.triangle
	double triangle(JavaRandom& random, double mode, double deviation) { return mode + deviation * (random.nextDouble() - random.nextDouble()); }

	// Blaze.BlazeAttackGoal
	class BlazeAttackGoal : public Goal {
	  public:
		explicit BlazeAttackGoal(Blaze& blaze) : _blaze(blaze) { setFlags({Flag::Move, Flag::Look}); }
		bool canUse() override {
			Actor* target = _blaze.getTarget();
			return target && target->isAlive() && _blaze.canAttack(*target);
		}
		void start() override { _attackStep = 0; }
		void stop() override {
			_blaze.setCharged(false);
			_lastSeen = 0;
		}
		bool requiresUpdateEveryTick() override { return true; }
		void tick() override {
			_attackTime--;
			Actor* target = _blaze.getTarget();
			if (!target) return;
			bool visible = _blaze.sensing().hasLineOfSight(*target);
			_lastSeen	 = visible ? 0 : _lastSeen + 1;
			double distance = _blaze.distanceToSqr(*target);
			double follow	= _blaze.getAttributeValue(_blaze.attributeIds().followRange);
			if (distance < 4.0) {
				if (!visible) return;
				if (_attackTime <= 0) {
					_attackTime = 20;
					_blaze.doHurtTarget(*target);
				}
				_blaze.moveControl().setWantedPosition(target->position().x, target->position().y, target->position().z, 1.0);
			} else if (distance < follow * follow && visible) {
				AABB   box = target->boundingBox();
				double dx  = target->position().x - _blaze.position().x;
				double dy  = target->position().y + (box.maxY - box.minY) * 0.5 - (_blaze.position().y + _blaze.height() * 0.5);
				double dz  = target->position().z - _blaze.position().z;
				if (_attackTime <= 0) {
					_attackStep++;
					if (_attackStep == 1) {
						_attackTime = 60;
						_blaze.setCharged(true);
					} else if (_attackStep <= 4) {
						_attackTime = 6;
					} else {
						_attackTime = 100;
						_attackStep = 0;
						_blaze.setCharged(false);
					}
					if (_attackStep > 1) {
						double spread = std::sqrt(std::sqrt(distance)) * 0.5;
						_blaze.level().levelEvent(nullptr, LEVEL_EVENT_BLAZE_SHOOT, _blaze.blockPosition(), 0);
						JavaRandom& random = _blaze.random();
						double		x	   = triangle(random, dx, 2.297 * spread);
						double		z	   = triangle(random, dz, 2.297 * spread);
						auto		ball   = std::make_unique<SmallFireball>(_blaze.level(), &_blaze, Vec3{x, dy, z}.normalize());
						ball->setPos({ball->position().x, _blaze.position().y + _blaze.height() * 0.5 + 0.5, ball->position().z});
						_blaze.level().entities().add(std::move(ball));
					}
				}
				_blaze.lookControl().setLookAt(*target, 10.0F, 10.0F);
			} else if (_lastSeen < 5) {
				_blaze.moveControl().setWantedPosition(target->position().x, target->position().y, target->position().z, 1.0);
			}
		}

	  private:
		Blaze& _blaze;
		int	   _attackStep = 0, _attackTime = 0, _lastSeen = 0;
	};
} // namespace

Blaze::Blaze(Level& level, int typeId) : Monster(level, typeId) {
	setPathfindingMalus(PathType::Water, -1.0F);
	setPathfindingMalus(PathType::Lava, 8.0F);
	setPathfindingMalus(PathType::DangerFire, 0.0F);
	setPathfindingMalus(PathType::DamageFire, 0.0F);
	_xpReward = 10;
}

void Blaze::registerGoals() {
	_goalSelector.addGoal(4, std::make_unique<BlazeAttackGoal>(*this));
	_goalSelector.addGoal(5, std::make_unique<MoveTowardsRestrictionGoal>(*this, 1.0));
	_goalSelector.addGoal(7, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0, 0.0F));
	_goalSelector.addGoal(8, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(8, std::make_unique<RandomLookAroundGoal>(*this));
	auto hurtBy = std::make_unique<HurtByTargetGoal>(*this);
	hurtBy->setAlertOthers();
	_targetSelector.addGoal(1, std::move(hurtBy));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

void Blaze::setCharged(bool charged) {
	uint8_t flags = static_cast<uint8_t>(charged ? _flags | 1 : _flags & ~1);
	if (flags == _flags) return;
	_flags = flags;
	markData(DATA_FLAGS);
}

void Blaze::aiStep() {
	// It falls slowly
	if (!_onGround && _delta.y < 0.0) _delta = _delta.multiply(1.0, 0.6, 1.0);
	Monster::aiStep();
}

void Blaze::customServerAiStep() {
	if (--_nextHeightOffsetChangeTick <= 0) {
		_nextHeightOffsetChangeTick = 100;
		_allowedHeightOffset		= static_cast<float>(triangle(_random, 0.5, 6.891));
	}
	// It rises above its target
	Actor* target = getTarget();
	if (target && target->eyeY() > eyeY() + _allowedHeightOffset && canAttack(*target)) {
		_delta.y += (0.3F - _delta.y) * 0.3F;
		hasImpulse = true;
	}
	Monster::customServerAiStep();
}

void Blaze::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_FLAGS, onlyNonDefault, _flags == 0)) writeByteData(buf, DATA_FLAGS, _flags);
}
