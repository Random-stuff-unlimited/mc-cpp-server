#include "world/entity/mobs/Blaze.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/SmallFireball.hpp"
#include "world/entity/ai/Goals.hpp"

#include <cmath>

namespace {
	// BlazeAttackGoal: hovers, and when it can see the target, shoots a volley of three fireballs every so often
	class BlazeAttackGoal : public Goal {
	  public:
		explicit BlazeAttackGoal(Blaze& blaze) : _blaze(blaze) {
			setFlags({Flag::Move, Flag::Look});
		}
		bool canUse() override { return _blaze.getTarget() && _blaze.getTarget()->isAlive(); }
		bool canContinueToUse() override { return canUse(); }
		void tick() override {
			Actor* target = _blaze.getTarget();
			if (!target) return;
			_blaze.lookControl().setLookAt(*target, 30.0F, 30.0F);
			if (!_blaze.sensing().hasLineOfSight(*target)) return;
			if (--_attackTicks > 0) return;
			// A volley of three, one every 3 ticks
			if (_volleyTicks <= 0) {
				_attackTicks = 20 + _blaze.random().nextInt(10);
				_volleyTicks = 3;
			} else {
				_volleyTicks--;
				_attackTicks = 3;
			}
			// The fireball toward the target's body, at 0.8 blocks/tick (SmallFireballEntity velocity)
			double dx = target->position().x - _blaze.position().x;
			double dy = target->position().y + 0.5 - (_blaze.position().y + _blaze.eyeHeight());
			double dz = target->position().z - _blaze.position().z;
			Vec3	dir{dx, dy, dz};
			double len = dir.length();
			if (len <= 1.0E-4) return;
			dir = dir.scale(0.8 / len);
			_blaze.level().entities().add(std::make_unique<SmallFireball>(
					_blaze.level(), Vec3{_blaze.position().x, _blaze.position().y + _blaze.eyeHeight(), _blaze.position().z}, dir, &_blaze));
			_blaze.level().playSoundAt(nullptr, _blaze.position().x, _blaze.position().y, _blaze.position().z, "minecraft:entity.blaze.shoot",
									   Level::SoundSource::Hostile, 1.0F, 1.0F);
		}

	  private:
		Blaze& _blaze;
		int	   _attackTicks = 0;
		int	   _volleyTicks = 0;
	};
} // namespace

Blaze::Blaze(Level& level, int typeId) : Monster(level, typeId) {
	// Blaze: it hovers, no gravity
	if (AttributeInstance* gravity = attributes().getInstance(attributeIds().gravity)) gravity->setBaseValue(0.0);
	_flyingAnimal = true;
}

void Blaze::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<BlazeAttackGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
}

void Blaze::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_ID_FLAGS, onlyNonDefault, true)) writeByteData(buf, DATA_ID_FLAGS, 0);
}