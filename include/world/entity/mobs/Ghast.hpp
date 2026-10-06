#ifndef GHAST_HPP
#define GHAST_HPP

#include "world/entity/Mob.hpp"

// Ghast: floats at random through the Nether (no gravity, its own move control), and from up to 64 blocks charges for
// a second then shoots an exploding fireball at the players it sees, every 3 seconds. Its own fireball sent back by a
// player kills it
class Ghast : public Mob {
  public:
	static constexpr int DATA_IS_CHARGING = 16;

	Ghast(Level& level, int typeId);

	bool isCharging() const { return _charging; }
	void setCharging(bool charging);
	int	 explosionPower() const { return _explosionPower; }
	// LivingEntity.getViewVector
	Vec3 viewVector() const;
	// Ghast.faceMovementDirection: toward its target within 64 blocks, else where it goes
	static void faceMovementDirection(Mob& mob);

	void registerGoals() override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	bool onClimbable() override { return false; }
	int	 getMaxSpawnClusterSize() const override { return 1; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void  travel(const Vec3& input) override { travelFlying(input, 0.02F); }
	void  checkFallDamage(double, bool, const BlockPos&) override {}
	float soundVolume() const override { return 5.0F; }
	void  writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _charging		 = false;
	int	 _explosionPower = 1;
};

#endif
