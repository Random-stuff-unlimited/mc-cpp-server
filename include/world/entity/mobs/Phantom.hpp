#ifndef PHANTOM_HPP
#define PHANTOM_HPP

#include "world/entity/mobs/Monster.hpp"

// Endermite: a small hostile that lives two minutes (endermen attack it)
class Endermite : public Monster {
  public:
	Endermite(Level& level, int typeId) : Monster(level, typeId) { _xpReward = 3; }
	void registerGoals() override;
	void tick() override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void aiStep() override;

  private:
	int _life = 0;
};

// Phantom: flies (its own move control: it glides toward a point, turning slowly) in circles around an anchor above
// its target, then swoops down on it every 8 to 12 seconds; burns in daylight, flees cats. They come at night for the
// players that haven't slept for three days (PhantomSpawner)
class Phantom : public Mob {
  public:
	static constexpr int DATA_SIZE = 16;
	enum class AttackPhase { Circle, Swoop };

	Phantom(Level& level, int typeId);

	int	 phantomSize() const { return _size; }
	void setPhantomSize(int size);

	// What its goals and move control share
	Vec3		moveTargetPoint{};
	bool		hasAnchor = false;
	BlockPos	anchorPoint;
	AttackPhase attackPhase = AttackPhase::Circle;

	void							registerGoals() override;
	bool							canAttackType(int) const override { return true; }
	bool							onClimbable() override { return false; }
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	void aiStep() override;
	void travel(const Vec3& input) override { travelFlying(input, 0.2F); }
	void checkFallDamage(double, bool, const BlockPos&) override {}
	// Phantom.PhantomBodyRotationControl
	void tickHeadTurn(float) override {
		_yHeadRot = _yBodyRot;
		_yBodyRot = _yRot;
	}
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	int _size = 0;
};

#endif
