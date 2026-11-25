#ifndef LONG_JUMP_GOAL_HPP
#define LONG_JUMP_GOAL_HPP

#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/ai/Goal.hpp"

#include <cmath>
#include <string>

// LongJumpToRandomPos and LongJumpMidJump (goats and frogs): now and then, on the ground, it picks a spot it can land
// on within `width` blocks around and `height` up or down, and leaps there on an arc (65 to 85 degrees, at most
// `maxVelocity`), in the long jumping pose until it lands
class LongJumpGoal : public Goal {
  public:
	LongJumpGoal(Mob& mob, int minCooldown, int maxCooldown, int height, int width, float maxVelocity, std::string sound)
		: _mob(mob), _minCooldown(minCooldown), _maxCooldown(maxCooldown), _height(height), _width(width), _maxVelocity(maxVelocity), _sound(std::move(sound)) {
		setFlags({Flag::Move, Flag::Look, Flag::Jump});
		_cooldown = _mob.random().nextInt(maxCooldown - minCooldown + 1) + minCooldown;
	}

	bool canUse() override {
		if (_cooldown > 0) {
			_cooldown--;
			return false;
		}
		if (!_mob.onGround() || _mob.isInWaterNow() || _mob.isLeashed() || _mob.hasRider()) return false;
		Level&		level  = _mob.level();
		JavaRandom& random = _mob.random();
		BlockPos	at	   = _mob.blockPosition();
		for (int attempt = 0; attempt < 20; attempt++) {
			BlockPos to = at.offset(random.nextInt(_width * 2 + 1) - _width, random.nextInt(_height * 2 + 1) - _height, random.nextInt(_width * 2 + 1) - _width);
			double	 dx = to.x + 0.5 - _mob.position().x, dz = to.z + 0.5 - _mob.position().z;
			if (dx * dx + dz * dz < 4.0) continue; // isAcceptableJumpPosition: not right next to it
			if (!level.gameData().getStateProperties(level.getBlockState(to.below())).blocksMotion) continue;
			AABB landing{to.x + 0.5 - _mob.width() / 2.0, static_cast<double>(to.y), to.z + 0.5 - _mob.width() / 2.0, to.x + 0.5 + _mob.width() / 2.0,
						 to.y + _mob.height(), to.z + 0.5 + _mob.width() / 2.0};
			if (level.hasBlockCollision(landing) || !level.getFluidState(to).isEmpty()) continue;
			if (jumpVector(to)) {
				_target = to;
				return true;
			}
		}
		_cooldown = 20; // Nothing found: look again in a second
		return false;
	}
	bool canContinueToUse() override { return _phase != Phase::Done; }
	void start() override {
		_phase = Phase::Crouching;
		_ticks = 0;
		_mob.navigation().stop();
		_mob.lookControl().setLookAt(_target.x + 0.5, _target.y + 0.5, _target.z + 0.5);
	}
	void stop() override {
		_mob.setPose(LivingEntity::POSE_STANDING);
		_cooldown = _mob.random().nextInt(_maxCooldown - _minCooldown + 1) + _minCooldown;
		_phase	  = Phase::Done;
	}
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override {
		_ticks++;
		if (_phase == Phase::Crouching) {
			// A short wind-up, facing the spot
			_mob.lookControl().setLookAt(_target.x + 0.5, _target.y + 0.5, _target.z + 0.5);
			if (_ticks < 10) return;
			if (!jumpVector(_target)) {
				_phase = Phase::Done;
				return;
			}
			_mob.setYRot(static_cast<float>(std::atan2(_velocity.z, _velocity.x) * 180.0 / M_PI) - 90.0F);
			_mob.setDeltaMovement(_velocity);
			_mob.setPose(POSE_LONG_JUMPING);
			_mob.level().playSoundAt(nullptr, _mob.position().x, _mob.position().y, _mob.position().z, _sound, Level::SoundSource::Neutral, 1.0F, 1.0F);
			_phase = Phase::Jumping;
			_ticks = 0;
			return;
		}
		// LongJumpMidJump: until it lands (or gives up)
		if ((_ticks > 3 && (_mob.onGround() || _mob.isInWaterNow())) || _ticks > 100) _phase = Phase::Done;
	}

  private:
	static constexpr int POSE_LONG_JUMPING = 6;
	enum class Phase { Crouching, Jumping, Done };
	Mob&		_mob;
	int			_minCooldown, _maxCooldown, _height, _width;
	float		_maxVelocity;
	std::string _sound;
	int			_cooldown = 0, _ticks = 0;
	Phase		_phase = Phase::Done;
	BlockPos	_target;
	Vec3		_velocity;

	// LongJumpUtil.calculateJumpVectorForAngle: the speed that lands it there at one of the angles (gravity 0.08)
	bool jumpVector(const BlockPos& to) {
		double dx = to.x + 0.5 - _mob.position().x, dz = to.z + 0.5 - _mob.position().z, dy = to.y - _mob.position().y;
		double distance = std::sqrt(dx * dx + dz * dz);
		if (distance < 1e-6) return false;
		for (int angle = 65; angle <= 85; angle += 5) {
			double theta = angle * M_PI / 180.0, tan = std::tan(theta), cos = std::cos(theta);
			double rise	 = distance * tan - dy;
			if (rise <= 0.0) continue;
			double speed = std::sqrt(0.08 * distance * distance / (2.0 * cos * cos * rise));
			if (!std::isfinite(speed) || speed > _maxVelocity) continue;
			double horizontal = speed * cos;
			_velocity		  = Vec3{dx / distance * horizontal, speed * std::sin(theta), dz / distance * horizontal};
			return true;
		}
		return false;
	}
};

#endif
