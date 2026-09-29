#ifndef SLIME_HPP
#define SLIME_HPP

#include "world/entity/Mob.hpp"

// Slime (and MagmaCube): a bouncy mob that jumps around in the direction it faces, splits into two smaller ones
// when it dies (down to size 1), and deals contact damage scaled by its size (1, 2 or 4). Magma cubes are the
// Nether's fire-immune version.
class Slime : public Mob {
  public:
	Slime(Level& level, int typeId);

	int size() const { return _size; }
	void setSize(int size);
	float targetYaw() const { return _targetYaw; }
	void setTargetYaw(float yaw) { _targetYaw = yaw; }

	void registerGoals() override;
	void tick() override;
	void die(const Combat::DamageSource& source) override;
	void dealContactDamage(Actor& target);
	bool fireImmune() const override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  private:
	static constexpr int DATA_SIZE = 16;

	int		_size	   = 2;
	float	_targetYaw = 0.0F;
};

#endif