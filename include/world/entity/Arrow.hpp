#ifndef ARROW_HPP
#define ARROW_HPP

#include "world/entity/MobEffects.hpp"
#include "world/entity/Projectile.hpp"
#include "world/item/ItemStack.hpp"

#include <memory>
#include <vector>

class LivingEntity;
namespace Clip {
	struct HitResult;
}

// An arrow (vanilla's AbstractArrow and Arrow): shot with a speed, it falls (0.05) and slows down (0.99 in the air,
// 0.6 in water), hurts the first entity along its path for ceil(speed * base damage) (more when critical), then sticks
// in the block it hits, where it can be picked up (if its pickup allows) and despawns after a minute. It can carry
// effects (tipped arrows, the stray's and bogged's)
class Arrow : public Projectile {
  public:
	enum class Pickup : uint8_t { Disallowed, Allowed, CreativeOnly };
	// Entity data (AbstractArrow and Arrow)
	static constexpr int DATA_FLAGS = 8, DATA_PIERCE_LEVEL = 9, DATA_IN_GROUND = 10, DATA_EFFECT_COLOR = 11;
	static constexpr int FLAG_CRIT = 1, FLAG_NO_PHYSICS = 2;
	static constexpr double DEFAULT_BASE_DAMAGE = 2.0;

	// type and defaultItem: "minecraft:arrow" (tridents are AbstractArrows too)
	explicit Arrow(Level& level, const char* type = "minecraft:arrow", const char* defaultItem = "minecraft:arrow");
	// AbstractArrow(type, owner, level, pickupItem, weapon): at the shooter's eyes (less 0.1), owned by it
	Arrow(Level& level, LivingEntity& shooter, ItemStack pickupItem, const ItemStack* weapon, const char* type = "minecraft:arrow",
		  const char* defaultItem = "minecraft:arrow");

	void		 tick() override;
	void		 setOwner(Actor* owner) override;
	void		 shoot(double x, double y, double z, float velocity, float inaccuracy) override;
	// AbstractArrow.setBaseDamageFromMob: the bow's power, and a bonus by difficulty
	void		 setBaseDamageFromMob(float power);
	void		 setBaseDamage(double damage) { _baseDamage = damage; }
	double		 baseDamage() const { return _baseDamage; }
	void		 setCritArrow(bool crit) { setFlag(FLAG_CRIT, crit); }
	bool		 isCritArrow() const { return _flags & FLAG_CRIT; }
	bool		 isInGround() const { return _inGround; }
	Pickup		 pickup() const { return _pickup; }
	void		 setPickup(Pickup pickup) { _pickup = pickup; }
	// Arrow.addEffect: an effect given to what it hits
	void		 addEffect(const MobEffects::Instance& effect);
	// AbstractArrow.playerTouch: a player takes it back once it stuck (and stopped shaking); the count taken
	int			 touchedByPlayer(Player& player) override;

	void writeEntityData(Buffer& buf) const override;
	bool shouldBeSaved() const override { return true; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	virtual void  onHitEntity(Actor& target);
	virtual float waterInertia() const { return 0.6F; }
	void		  deflectReverse();
	bool		  canHitEntity(Actor& target) override;

	bool	  _hasEffectColorData = true; // Arrow.ID_EFFECT_COLOR (a trident's data 11 is its loyalty: left at its default)
	int		  _life = 0, _inGroundTime = 0, _shakeTime = 0;
	bool	  _inGround = false;
	uint8_t	  _flags = 0, _pierceLevel = 0;
	int		  _lastState = -1; // The block it stuck in (-1: none)
	double	  _baseDamage = DEFAULT_BASE_DAMAGE;
	Pickup	  _pickup	  = Pickup::Disallowed;
	ItemStack _pickupItem, _firedFromWeapon;
	bool	  _hasWeapon = false;
	std::string _soundEvent = "minecraft:entity.arrow.hit";
	std::vector<MobEffects::Instance> _effects;
	int								  _effectColor = -1;
	std::vector<int>				  _piercingIgnore; // Ids of the entities a piercing arrow went through

	void setFlag(int flag, bool value);
	void setInGround(bool inGround);
	bool shouldFall();
	void startFalling();
	void applyFireEffectsFromBlocks();
	bool isInWaterOrRain();
	void stepMoveAndHit(const Clip::HitResult& block);
	void onHitBlock(const BlockPos& pos, const Vec3& location);
	void updateEffectColor();
};

// ThrownTrident: 8 damage, then it falls where it hit (it only hits once). The loyalty enchantment isn't ported
class ThrownTrident : public Arrow {
  public:
	ThrownTrident(Level& level, LivingEntity& shooter, ItemStack trident);
	explicit ThrownTrident(Level& level);
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	void  onHitEntity(Actor& target) override;
	float waterInertia() const override { return 0.99F; }
	bool  canHitEntity(Actor& target) override { return !_dealtDamage && Arrow::canHitEntity(target); }

  private:
	bool _dealtDamage = false;
};

#endif
