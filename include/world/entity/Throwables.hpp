#ifndef THROWABLES_HPP
#define THROWABLES_HPP

#include "world/entity/Projectile.hpp"
#include "world/item/ItemStack.hpp"

class LivingEntity;
class Player;

// ThrowableProjectile / ThrowableItemProjectile: a thrown item. Each tick: gravity, drag (0.99, 0.8 in water), then
// it moves to the first block or entity along its movement and hits it (onHit). Its item is its entity data
class ThrowableProjectile : public Projectile {
  public:
	static constexpr int DATA_ITEM_STACK = 8;
	// What it hit: an entity (actor), or a block (pos and face)
	struct Hit {
		Actor*	  actor = nullptr;
		Vec3	  location;
		BlockPos  pos;
		Direction face = Direction::Up;
	};

	// At the thrower's eyes (less 0.1), owned by it
	ThrowableProjectile(Level& level, const char* type, Actor* thrower, ItemStack item);

	const ItemStack& item() const { return _item; }
	void			 tick() override;
	// Projectile.shootFromRotation: along a look (pitch, yaw), raised by `offset` degrees
	void			 shootFromRotation(float xRot, float yRot, float offset, float velocity, float inaccuracy);
	void			 writeEntityData(Buffer& buf) const override;

  protected:
	ItemStack _item;

	virtual double gravity() const { return 0.03; }
	// Projectile.onHit
	virtual void   onHit(const Hit& hit) = 0;
	// EntityEvent 3 for its viewers (the break particles), then gone
	void		   breakAndDiscard();
};

// Snowball: no damage (3 to a blaze), a knockback from the hit
class Snowball : public ThrowableProjectile {
  public:
	Snowball(Level& level, Actor* thrower, ItemStack item = {});

  protected:
	void onHit(const Hit& hit) override;
};

// ThrownEgg: sometimes hatches a chick (1/8), rarely four (1/256)
class ThrownEgg : public ThrowableProjectile {
  public:
	ThrownEgg(Level& level, Actor* thrower, ItemStack item = {});

  protected:
	void onHit(const Hit& hit) override;
};

// ThrownExperienceBottle: breaks into 3 to 11 experience
class ThrownExperienceBottle : public ThrowableProjectile {
  public:
	ThrownExperienceBottle(Level& level, Actor* thrower, ItemStack item = {});

  protected:
	double gravity() const override { return 0.07; }
	void   onHit(const Hit& hit) override;
};

// ThrownSplashPotion (AbstractThrownPotion): breaks and gives its potion's effects to the living within 4 blocks, less
// the farther they are; a water bottle puts out fires and hurts what water hurts
class ThrownSplashPotion : public ThrowableProjectile {
  public:
	ThrownSplashPotion(Level& level, Actor* thrower, ItemStack item);

  protected:
	double gravity() const override { return 0.05; }
	void   onHit(const Hit& hit) override;

  private:
	void onHitAsWater();
	void onHitAsPotion(const std::string& potion, float durationScale, const Vec3& location);
	void dowseFire(const BlockPos& pos);
};

#endif
