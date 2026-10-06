#ifndef FIREBALLS_HPP
#define FIREBALLS_HPP

#include "world/entity/Projectile.hpp"
#include "world/entity/Throwables.hpp"

class LivingEntity;

// AbstractHurtingProjectile / Fireball: flies straight, speeding up along its movement (accelerationPower) against a
// drag (0.95, 0.8 in water), burning, until it hits a block or an entity. Gone when its owner is
class HurtingProjectile : public Projectile {
  public:
	using Hit = ThrowableProjectile::Hit;

	// At the shooter, toward `direction` (normalized), turned like it
	HurtingProjectile(Level& level, const char* type, float size, LivingEntity* shooter, const Vec3& direction);

	void   tick() override;
	void   writeEntityData(Buffer& buf) const override;
	double accelerationPower = 0.1;
	// Projectile.deflect(AIM_DEFLECT): hit back by a player, along its look
	void   deflectAim(Actor& by, const Vec3& look);

  protected:
	virtual void onHit(const Hit& hit) = 0;
};

// SmallFireball (blazes): 5 damage and 5 seconds of fire, lights a fire where it lands
class SmallFireball : public HurtingProjectile {
  public:
	SmallFireball(Level& level, LivingEntity* shooter, const Vec3& direction);

  protected:
	void onHit(const Hit& hit) override;
};

// LargeFireball (ghasts): 6 damage, then explodes (with fire)
class LargeFireball : public HurtingProjectile {
  public:
	LargeFireball(Level& level, LivingEntity* shooter, const Vec3& direction, int explosionPower);

  protected:
	void onHit(const Hit& hit) override;

  private:
	int _explosionPower;
};

#endif
