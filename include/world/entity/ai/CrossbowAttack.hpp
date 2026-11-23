#ifndef CROSSBOW_ATTACK_HPP
#define CROSSBOW_ATTACK_HPP

#include "world/entity/Mob.hpp"
#include "world/entity/ai/Goal.hpp"

class Level;
class Mob;


// CrossbowAttackMob: the mobs that shoot crossbows (pillagers, piglins)
class CrossbowAttackMob {
  public:
	virtual ~CrossbowAttackMob() = default;
	virtual void setChargingCrossbow(bool charging) = 0;
	virtual bool isHoldingCrossbow()				= 0;
};

bool isCrossbow(Level& level, const ItemStack& stack);
// The crossbow's loading_end sound, at the mob
void playCrossbowLoaded(Mob& mob);

// RangedCrossbowAttackGoal: walks up to its target, loads its crossbow (25 ticks), waits one or two seconds and fires
// once it sees it (Mob.performRangedAttack)
class RangedCrossbowAttackGoal : public Goal {
  public:
	RangedCrossbowAttackGoal(Mob& mob, CrossbowAttackMob& user, double speed, float range)
		: _mob(mob), _user(user), _speedModifier(speed), _attackRadiusSqr(range * range) {
		setFlags({Flag::Move, Flag::Look});
	}
	bool canUse() override { return isValidTarget() && _user.isHoldingCrossbow(); }
	bool canContinueToUse() override { return isValidTarget() && (canUse() || !_mob.navigation().isDone()) && _user.isHoldingCrossbow(); }
	void start() override { _mob.setAggressive(true); }
	void stop() override {
		_mob.setAggressive(false);
		_mob.setTarget(nullptr);
		_seeTime = 0;
		if (_mob.isUsingItem()) {
			_mob.stopUsingItem();
			_user.setChargingCrossbow(false);
		}
		_state = State::Uncharged;
	}
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override {
		Actor* target = _mob.getTarget();
		if (!target) return;
		bool hasLineOfSight = _mob.sensing().hasLineOfSight(*target);
		if (hasLineOfSight != (_seeTime > 0)) _seeTime = 0;
		_seeTime += hasLineOfSight ? 1 : -1;
		double distance	 = _mob.distanceToSqr(*target);
		bool   approach = (distance > _attackRadiusSqr || _seeTime < 5) && _attackDelay == 0;
		if (approach) {
			if (--_updatePathDelay <= 0) {
				_mob.navigation().moveTo(*target, _state == State::Uncharged ? _speedModifier : _speedModifier * 0.5);
				_updatePathDelay = 20 + _mob.random().nextInt(21); // PATHFINDING_DELAY_RANGE
			}
		} else {
			_updatePathDelay = 0;
			_mob.navigation().stop();
		}
		_mob.lookControl().setLookAt(*target, 30.0F, 30.0F);
		switch (_state) {
			case State::Uncharged:
				if (!approach) {
					_mob.startUsingItem(isCrossbow(_mob.level(), _mob.getMainHandItem()) ? 0 : 1);
					_state = State::Charging;
					_user.setChargingCrossbow(true);
				}
				break;
			case State::Charging:
				if (!_mob.isUsingItem()) _state = State::Uncharged;
				if (_mob.getTicksUsingItem() >= 25) { // CrossbowItem.getChargeDuration, without quick charge
					_mob.stopUsingItem();
					_state		 = State::Charged;
					_attackDelay = 20 + _mob.random().nextInt(20);
					_user.setChargingCrossbow(false);
					playCrossbowLoaded(_mob);
				}
				break;
			case State::Charged:
				if (--_attackDelay <= 0) {
					_attackDelay = 0;
					_state		 = State::ReadyToAttack;
				}
				break;
			case State::ReadyToAttack:
				if (hasLineOfSight) {
					_mob.performRangedAttack(*target, 1.0F);
					_state = State::Uncharged;
				}
				break;
		}
	}

  private:
	enum class State { Uncharged, Charging, Charged, ReadyToAttack };
	Mob&			   _mob;
	CrossbowAttackMob& _user;
	State	  _state = State::Uncharged;
	double	  _speedModifier;
	float	  _attackRadiusSqr;
	int		  _seeTime = 0, _attackDelay = 0, _updatePathDelay = 0;

	bool isValidTarget() {
		Actor* target = _mob.getTarget();
		return target && target->isAlive();
	}
};

#endif
