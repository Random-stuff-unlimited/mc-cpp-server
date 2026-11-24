#ifndef AREA_EFFECT_CLOUD_HPP
#define AREA_EFFECT_CLOUD_HPP

#include "world/entity/Entity.hpp"
#include "world/entity/MobEffects.hpp"

#include <map>
#include <string>
#include <vector>

class Player;

// AreaEffectCloud: a flat cloud that gives its effects to the living inside it, every 5 ticks, each at most once a
// second. Lingering potions leave one (3 blocks wide, shrinking, smaller each time it touches someone); the ender
// dragon's fireballs too (instant damage, growing to 7 blocks), from which a glass bottle collects dragon's breath
class AreaEffectCloud : public Entity {
  public:
	static constexpr int DATA_RADIUS = 8, DATA_WAITING = 9, DATA_PARTICLE = 10;

	explicit AreaEffectCloud(Level& level);
	AreaEffectCloud(Level& level, const Vec3& position);

	// ThrownLingeringPotion.onHitAsPotion: the cloud of a lingering potion (registry name), owned by its thrower
	static std::unique_ptr<AreaEffectCloud> lingering(Level& level, const Vec3& position, const std::string& potion, Actor* owner);
	// DragonFireball.onHit: the dragon's breath
	static std::unique_ptr<AreaEffectCloud> dragonBreath(Level& level, const Vec3& position, Actor* owner);

	float radius() const { return _radius; }
	void  setRadius(float radius);
	bool  isDragonBreath() const { return _dragonBreath; }
	bool  isWaiting() const { return _waiting; }
	int	  duration() const { return _duration; }
	// The effects it gives (its potion's, or its own), before potion_duration_scale
	std::vector<MobEffects::Instance> effects() const;

	void tick() override;
	AABB boundingBox() const override;
	void writeEntityData(Buffer& buf) const override;
	bool shouldBeSaved() const override { return true; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  private:
	float		_radius = 3.0F, _radiusOnUse = 0.0F, _radiusPerTick = 0.0F;
	int			_duration = -1, _durationOnUse = 0, _waitTime = 20, _reapplicationDelay = 20;
	float		_durationScale = 1.0F; // potion_duration_scale
	bool		_waiting	   = true;
	std::string _potion;				// minecraft:potion registry name, empty for none
	bool		_dragonBreath = false;	// Instant damage II, purple particles
	Actor*		_owner		  = nullptr;
	int			_ownerId	  = -1;
	std::map<int, int> _victims;		// Entity id -> tick it may be affected again

	int	 particleColor() const;
	// Its effects on one target; false when the cloud is gone after it (too small)
	bool affect(Actor& target, MobEffects::Host& host, MobEffects::Active& effects, const std::vector<MobEffects::Instance>& list);
	Actor* owner();
};

#endif
