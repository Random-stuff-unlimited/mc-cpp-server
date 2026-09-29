#include "world/entity/mobs/Bat.hpp"

#include "network/buffer.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Controls.hpp"

#include <cmath>

namespace {
	// BatMoveControl: flies straight toward the wanted point, wobbling a little, at about 0.2 blocks a tick
	class BatMoveControl : public MoveControl {
	  public:
		explicit BatMoveControl(Bat& bat) : MoveControl(bat), _bat(bat) {}
		void tick() override {
			double dx = wantedX() - _bat.position().x;
			double dy = wantedY() - _bat.position().y;
			double dz = wantedZ() - _bat.position().z;
			double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (dist < 0.2) {
				_bat.setDeltaMovement({0.0, 0.0, 0.0});
				return;
			}
			// The wobble (BatMoveControl): a small perpendicular drift
			_bat.setDeltaMovement({dx / dist * 0.2, dy / dist * 0.2, dz / dist * 0.2});
			_bat.setYRotPublic(static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI) - 90.0F);
		}

	  private:
		Bat& _bat;
	};
	// BatHoverGoal: an idle bat picks a random point within 10 blocks and drifts toward it
	class BatHoverGoal : public Goal {
	  public:
		explicit BatHoverGoal(Bat& bat) : _bat(bat) {
			setFlags({Flag::Move});
		}
		bool canUse() override { return true; }
		void tick() override {
			if (--_ticks <= 0) {
				_ticks = 20 + _bat.random().nextInt(40);
				double x = _bat.position().x + (_bat.random().nextDouble() - 0.5) * 20.0;
				double y = _bat.position().y + (_bat.random().nextDouble() - 0.5) * 10.0;
				double z = _bat.position().z + (_bat.random().nextDouble() - 0.5) * 20.0;
				_bat.moveControl().setWantedPosition(x, y, z, 1.0);
			}
		}

	  private:
		Bat& _bat;
		int	 _ticks = 0;
	};
} // namespace

Bat::Bat(Level& level, int typeId) : Mob(level, typeId) {
	// Bat.travel: no gravity, and its vertical speed isn't damped like a falling mob's
	if (AttributeInstance* gravity = attributes().getInstance(attributeIds().gravity)) gravity->setBaseValue(0.0);
	_flyingAnimal = true;
	setMoveControl(std::make_unique<BatMoveControl>(*this));
}

void Bat::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<BatHoverGoal>(*this));
}

void Bat::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_ID_FLAGS, onlyNonDefault, true)) writeByteData(buf, DATA_ID_FLAGS, 0);
}