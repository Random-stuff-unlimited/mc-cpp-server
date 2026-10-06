#ifndef TAMABLE_ANIMAL_HPP
#define TAMABLE_ANIMAL_HPP

#include "world/entity/ai/Goals.hpp"
#include "world/entity/mobs/Animal.hpp"

// TamableAnimal: an animal a player can tame (wolves, cats, parrots). Tamed, it has an owner it follows (teleporting to
// it past 12 blocks), sits when ordered to, and never attacks it
class TamableAnimal : public Animal {
  public:
	static constexpr int DATA_FLAGS = 17, DATA_OWNER = 18; // Flags: 1 sitting, 4 tame
	static constexpr int EVENT_TAMING_FAILED = 6, EVENT_TAMING_SUCCEEDED = 7;

	using Animal::Animal;

	bool   isTame() const { return _tameFlags & 4; }
	// setTame: applySideEffects for a wolf's health
	virtual void setTame(bool tame, bool applySideEffects);
	bool   isInSittingPose() const { return _tameFlags & 1; }
	void   setInSittingPose(bool sitting);
	bool   isOrderedToSit() const { return _orderedToSit; }
	void   setOrderedToSit(bool sit) { _orderedToSit = sit; }
	// OwnableEntity.getOwner: its owner if it is in this level
	Actor* getOwner();
	Actor* getOwnerEntity() override { return getOwner(); }
	bool   hasOwner() const { return _owner.isSet(); }
	const EntityRef& ownerReference() const { return _owner; }
	void   setOwnerReference(const EntityRef& owner);
	bool   isOwnedBy(Actor& actor) { return getOwner() == &actor; }
	void   tame(Player& player);
	// TamableAnimal.wantsToAttack: whether it fights `target` for its owner
	virtual bool wantsToAttack(Actor& target, Actor& owner);
	bool   shouldTryTeleportToOwner();
	void   tryToTeleportToOwner();
	// unableToMoveToOwner: sitting, or its owner is a spectator
	bool   unableToMoveToOwner();

	bool canAttack(Actor& target) override;
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  protected:
	virtual void applyTamingSideEffects() {}
	virtual bool canFlyToOwner() const { return false; }
	void		 writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t	  _tameFlags = 0;
	EntityRef _owner;
	bool	  _orderedToSit = false;

	bool canTeleportTo(const BlockPos& pos);
};

// TamableAnimal.TamableAnimalPanicGoal: teleports to its owner while it flees, when far from it
class TamableAnimalPanicGoal : public PanicGoal {
  public:
	TamableAnimalPanicGoal(TamableAnimal& mob, double speed, std::string panicTag = "minecraft:panic_causes")
		: PanicGoal(mob, speed, std::move(panicTag)), _tamable(mob) {}
	void tick() override;

  private:
	TamableAnimal& _tamable;
};

// SitWhenOrderedToGoal: sits when told (and when its owner is away), unless its owner was just attacked nearby
class SitWhenOrderedToGoal : public Goal {
  public:
	explicit SitWhenOrderedToGoal(TamableAnimal& mob) : _mob(mob) { setFlags({Flag::Jump, Flag::Move}); }
	bool canUse() override;
	bool canContinueToUse() override { return _mob.isOrderedToSit(); }
	void start() override;
	void stop() override { _mob.setInSittingPose(false); }

  private:
	TamableAnimal& _mob;
};

// FollowOwnerGoal: walks to its owner when farther than startDistance, until within stopDistance
class FollowOwnerGoal : public Goal {
  public:
	FollowOwnerGoal(TamableAnimal& mob, double speed, float startDistance, float stopDistance)
		: _tamable(mob), _speedModifier(speed), _startDistance(startDistance), _stopDistance(stopDistance) {
		setFlags({Flag::Move, Flag::Look});
	}
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override;
	void tick() override;

  private:
	TamableAnimal& _tamable;
	EntityRef	   _owner;
	double		   _speedModifier;
	float		   _startDistance, _stopDistance, _oldWaterCost = 0;
	int			   _timeToRecalcPath = 0;
};

// OwnerHurtByTargetGoal / OwnerHurtTargetGoal: attacks what hurt its owner, and what its owner hit
class OwnerHurtByTargetGoal : public TargetGoal {
  public:
	explicit OwnerHurtByTargetGoal(TamableAnimal& mob, bool ownerAttacked = true) : TargetGoal(mob, false), _tamable(mob), _ownerAttacked(ownerAttacked) {
		setFlags({Flag::Target});
	}
	bool canUse() override;
	void start() override;

  private:
	TamableAnimal& _tamable;
	bool		   _ownerAttacked; // false: OwnerHurtTargetGoal
	EntityRef	   _wanted;
	int			   _timestamp = 0;
};

// NonTameRandomTargetGoal: wild ones only
class NonTameRandomTargetGoal : public NearestAttackableTargetGoal {
  public:
	NonTameRandomTargetGoal(TamableAnimal& mob, std::string targetClass, bool mustSee, std::function<bool(Actor&, Level&)> selector)
		: NearestAttackableTargetGoal(mob, std::move(targetClass), 10, mustSee, false, std::move(selector)), _tamable(mob) {}
	bool canUse() override { return !_tamable.isTame() && NearestAttackableTargetGoal::canUse(); }
	bool canContinueToUse() override;

  private:
	TamableAnimal& _tamable;
};

#endif
