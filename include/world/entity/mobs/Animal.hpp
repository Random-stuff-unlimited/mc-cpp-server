#ifndef ANIMAL_HPP
#define ANIMAL_HPP

#include "world/entity/Mob.hpp"

// AgeableMob: mobs that are born as babies (half their size) and grow up in 20 minutes (age -24000 counting up to 0).
// After breeding, the parents wait 5 minutes (age 6000 counting down). Some spawn as babies (5% of a pack's mobs
// after the first)
class AgeableMob : public Mob {
  public:
	static constexpr int DATA_BABY		= 16;
	static constexpr int BABY_START_AGE = -24000;

	AgeableMob(Level& level, int typeId) : Mob(level, typeId) {}

	bool isBaby() const override { return _age < 0; }
	void setBaby(bool baby) override { setAge(baby ? BABY_START_AGE : 0); }
	int	 getAge() const { return _age; }
	void setAge(int age);
	// AgeableMob.ageUp: `seconds` older (babies fed), forced: the green particles
	void ageUp(int seconds, bool forced = false);
	// AgeableMob.getSpeedUpSecondsWhenFeeding
	static int getSpeedUpSecondsWhenFeeding(int ticksUntilAdult) { return static_cast<int>(ticksUntilAdult / 20 * 0.1F); }

	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;
	void							save(Buffer& buf) const override;
	void							load(Buffer& buf) override;

  protected:
	int _age = 0, _forcedAge = 0, _forcedAgeTimer = 0;

	void		 aiStep() override;
	// AgeableMob.ageBoundaryReached: the size changes (riding boats isn't ported)
	virtual void ageBoundaryReached();
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;
	// The baby's eye height (half the adult's; chickens and a few others have their own)
	virtual float babyEyeHeight() const { return _type.eyeHeight * 0.5F; }
	// The baby's chance to spawn as one (AgeableMobGroupData's babySpawnChance)
	virtual float babySpawnChance() const { return 0.05F; }
};

// Animal: eats its food from the player's hand to fall in love (or grow faster as a baby), then breeds with another
// of its kind in love nearby; likes grass, never despawns
class Animal : public AgeableMob {
  public:
	static constexpr int PARENT_AGE_AFTER_BREEDING = 6000;
	static constexpr int EVENT_IN_LOVE_HEARTS	   = 18;

	Animal(Level& level, int typeId);

	// Animal.isFood: its breeding food (the "<type>_food" item tag of its class by default)
	virtual bool isFood(const ItemStack& stack) const;
	bool		 canFallInLove() const { return _inLove <= 0; }
	void		 setInLove(Player* cause);
	bool		 isInLove() const { return _inLove > 0; }
	int			 inLoveTime() const { return _inLove; }
	void		 resetLove() { _inLove = 0; }
	// Animal.canMate: another of the same class, both in love
	virtual bool canMate(Animal& other);
	// Animal.spawnChildFromBreeding: the baby at its position, the parents wait 5 minutes
	void		 spawnChildFromBreeding(Animal& partner);
	// Animal.getBreedOffspring: a baby of its own type (sheep mix their colors...)
	virtual std::unique_ptr<Mob> getBreedOffspring(Animal& partner);

	float getWalkTargetValue(const BlockPos& pos) override;
	bool  mobInteract(Player& player, int hand) override;
	bool  hurtServer(const Combat::DamageSource& source, float amount) override;
	void  save(Buffer& buf) const override;
	void  load(Buffer& buf) override;

  protected:
	int		  _inLove = 0;
	EntityRef _loveCause;

	void		 aiStep() override;
	void		 customServerAiStep() override;
	int			 ambientSoundInterval() const override { return 120; }
	virtual void playEatingSound() {}
	// Mob.usePlayerItem: one of the stack eaten (not in creative)
	void		 usePlayerItem(Player& player, int hand);
	// The tag of its food ("minecraft:cow_food")
	std::string	 foodTag() const;
};

// BreedGoal: in love, it walks to the closest partner in love within 8 blocks and breeds after 3 seconds together
class BreedGoal : public Goal {
  public:
	BreedGoal(Animal& animal, double speed);
	bool canUse() override;
	bool canContinueToUse() override;
	void stop() override;
	void tick() override;

  private:
	Animal&	  _animal;
	EntityRef _partner;
	int		  _loveTime = 0;
	double	  _speedModifier;

	Animal* partner();
	Animal* getFreePartner();
};

// FollowParentGoal: a baby follows the closest adult of its kind (from 3 to 16 blocks away)
class FollowParentGoal : public Goal {
  public:
	FollowParentGoal(Animal& animal, double speed) : _animal(animal), _speedModifier(speed) {}
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override { _timeToRecalcPath = 0; }
	void stop() override { _parent.clear(); }
	void tick() override;

  private:
	Animal&	  _animal;
	EntityRef _parent;
	double	  _speedModifier;
	int		  _timeToRecalcPath = 0;
};

// EatBlockGoal: sheep eat the grass they stand on (grass blocks become dirt), which regrows their wool
class EatBlockGoal : public Goal {
  public:
	explicit EatBlockGoal(Mob& mob);
	bool canUse() override;
	void start() override;
	void stop() override { _eatAnimationTick = 0; }
	bool canContinueToUse() override { return _eatAnimationTick > 0; }
	void tick() override;
	int	 eatAnimationTick() const { return _eatAnimationTick; }

  private:
	Mob& _mob;
	int	 _eatAnimationTick = 0;
};

#endif
