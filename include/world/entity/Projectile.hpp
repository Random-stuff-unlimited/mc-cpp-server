#ifndef PROJECTILE_HPP
#define PROJECTILE_HPP

#include "world/entity/Entity.hpp"

#include <optional>

// Vanilla's Projectile: an entity thrown or shot by an owner. It leaves its owner's box before it can hit it, and
// finds what it hits along its movement (ProjectileUtil.getEntityHitResult). Its owner's id goes with its spawn packet
class Projectile : public Entity {
  public:
	Projectile(Level& level, int typeId, float width, float height) : Entity(level, typeId, width, height) {}

	Actor* owner() const override;
	virtual void setOwner(Actor* owner) {
		if (owner) {
			_owner = EntityRef(*owner);
		} else {
			_owner.clear();
		}
	}
	// Projectile.getEffectSource: its owner, else itself
	Actor* effectSource() {
		Actor* o = owner();
		return o ? o : static_cast<Actor*>(this);
	}
	// Projectile.shoot: toward (x, y, z) at this speed, spread by the inaccuracy, turned to face its movement
	virtual void shoot(double x, double y, double z, float velocity, float inaccuracy);
	// ClientboundAddEntityPacket's data: the owner's id, 0 if none
	int			 entityData() const override;

  protected:
	EntityRef _owner;
	bool	  _leftOwner = false, _leftOwnerChecked = false, _hasBeenShot = false;

	// Projectile.tick: the owner check, then Entity.tick
	void		 tick() override;
	void		 checkLeftOwner();
	// Projectile.canHitEntity: something that can be hit (a living, pickable entity or player), its owner only once out of it
	virtual bool canHitEntity(Actor& target);

	struct EntityHit {
		Actor* actor;
		Vec3   location;
	};
	// ProjectileUtil.getEntityHitResult: the nearest hittable entity whose box (grown by margin) the segment enters
	std::optional<EntityHit> findHitEntity(const Vec3& from, const Vec3& to, const AABB& area, float margin);
	// ProjectileUtil.computeMargin: from 0 to 0.3 over its first ticks
	float					 computeMargin() const { return std::max(0.0F, std::min(0.3F, (_tickCount - 2) / 20.0F)); }
	// Projectile.lerpRotation
	static float			 lerpRotation(float from, float to);
	// Projectile.updateRotation: facing its movement, smoothly
	void					 updateRotation();

	// The owner and the flags of Projectile.addAdditionalSaveData
	void saveProjectile(Buffer& buf) const;
	void loadProjectile(Buffer& buf);
};

#endif
