#include "world/entity/mobs/SnowGolem.hpp"

#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/Snowball.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	// SnowGolem: throws a snowball at its target when it can see it
	class SnowGolemAttackGoal : public Goal {
	  public:
		explicit SnowGolemAttackGoal(SnowGolem& golem) : _golem(golem) {
			setFlags({Flag::Move, Flag::Look});
		}
		bool canUse() override { return _golem.getTarget() && _golem.getTarget()->isAlive(); }
		bool canContinueToUse() override { return canUse(); }
		void tick() override {
			Actor* target = _golem.getTarget();
			if (!target) return;
			_golem.lookControl().setLookAt(*target, 30.0F, 30.0F);
			if (!_golem.sensing().hasLineOfSight(*target)) return;
			if (--_attackTicks > 0) return;
			_attackTicks = 20 + _golem.random().nextInt(10);
			// Aimed a little above the target's feet, at 1.6 blocks/tick (snowball velocity)
			double dx = target->position().x - _golem.position().x;
			double dy = target->position().y - (_golem.position().y + _golem.eyeHeight());
			double dz = target->position().z - _golem.position().z;
			Vec3	dir{dx, dy, dz};
			double len = dir.length();
			if (len <= 1.0E-4) return;
			dir = dir.scale(1.6 / len);
			_golem.level().entities().add(std::make_unique<Snowball>(_golem.level(),
																	Vec3{_golem.position().x, _golem.position().y + _golem.eyeHeight(), _golem.position().z},
																	dir, &_golem));
			_golem.level().playSoundAt(nullptr, _golem.position().x, _golem.position().y, _golem.position().z, "minecraft:entity.snowball.throw",
									   Level::SoundSource::Neutral, 1.0F, 1.0F);
		}

	  private:
		SnowGolem& _golem;
		int		   _attackTicks = 0;
	};
} // namespace

void SnowGolem::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<SnowGolemAttackGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Monster", true));
}

void SnowGolem::tick() {
	// SnowGolem: rain melts it (1 damage a second, then it dies)
	if (isAlive() && _level.isRaining() && _level.canSeeSky(blockPosition())) {
		hurtServer({"minecraft:drown", nullptr, nullptr, std::nullopt}, 1.0F);
	}
	Mob::tick();
}

bool SnowGolem::hurtServer(const Combat::DamageSource& source, float amount) {
	// In warm biomes it melts; without biome temperatures, the rain above covers it
	return Mob::hurtServer(source, amount);
}