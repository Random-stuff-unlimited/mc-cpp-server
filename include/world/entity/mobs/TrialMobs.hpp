#ifndef TRIAL_MOBS_HPP
#define TRIAL_MOBS_HPP

#include "world/entity/Fireballs.hpp"
#include "world/entity/mobs/Monster.hpp"

// Breeze: a mob of the trial chambers. It shoots wind charges at its target (a 1 damage hit, then a burst that throws
// everything around), long-jumps around it, and projectiles bounce off it. The Brain is approximated by one goal
class Breeze : public Monster {
  public:
	static constexpr int POSE_LONG_JUMPING = 6, POSE_SLIDING = 15, POSE_SHOOTING = 16, POSE_INHALING = 17;

	Breeze(Level& level, int typeId);

	void		registerGoals() override;
	bool		hurtServer(const Combat::DamageSource& source, float amount) override;
	bool		causeFallDamage(double, float) override { return false; }
	std::string ambientSoundName() override { return _onGround ? "minecraft:entity.breeze.idle_ground" : "minecraft:entity.breeze.idle_air"; }
	using Monster::playSound;
	// Breeze.getSnoutYPosition: where its wind charges come from
	double snoutY() const { return _position.y + _height * 0.5 - 0.3; }
};

// BreezeWindCharge: flies straight without slowing down; what it hits takes 1 damage, then it bursts (a knockback
// explosion of radius 3 that breaks nothing)
class BreezeWindCharge : public HurtingProjectile {
  public:
	BreezeWindCharge(Level& level, Breeze* breeze);
	explicit BreezeWindCharge(Level& level) : BreezeWindCharge(level, nullptr) {}
	void writeEntityData(Buffer&) const override {}

  protected:
	float inertia() const override { return 1.0F; }
	float liquidInertia() const override { return 1.0F; }
	bool  shouldBurn() const override { return false; }
	void  onHit(const Hit& hit) override;

  private:
	void explode(const Vec3& at);
};

// Creaking: a monster of the pale gardens that freezes while a player looks at it, and otherwise walks up to its
// target and hits it. The creaking heart that protects it (and makes it invulnerable) isn't ported
class Creaking : public Monster {
  public:
	static constexpr int DATA_CAN_MOVE = 16, DATA_IS_ACTIVE = 17, DATA_IS_TEARING_DOWN = 18;
	static constexpr int EVENT_ATTACK = 4;

	Creaking(Level& level, int typeId);

	bool canMove() const { return _canMove; }
	bool isActive() const { return _active; }
	// Creaking.checkCanMove: no player (not creative nor spectator) within 24 blocks looks at it
	bool checkCanMove();

	void		registerGoals() override;
	bool		doHurtTarget(Actor& target) override;
	std::string ambientSoundName() override { return _active ? "minecraft:entity.creaking.ambient" : ""; }
	using Monster::playSound;

  protected:
	void customServerAiStep() override;
	void writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	bool _canMove = true, _active = false;
	int	 _attackAnimationRemainingTicks = 0;

	void setCanMove(bool can);
	void setActive(bool active);
};

#endif
