#ifndef GOALS_HPP
#define GOALS_HPP

#include "world/entity/Actor.hpp"
#include "world/entity/ai/Goal.hpp"
#include "world/entity/ai/Pathfinder.hpp"
#include "world/item/ItemStack.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Level;
class Mob;

// Vanilla's goals (net.minecraft.world.entity.ai.goal), ported one by one. A goal is given to a mob in its class's
// registerGoals or in MobGoals.cpp, with vanilla's priorities

// ----- Targeting (TargetingConditions, and Level.getNearestPlayer / getNearestEntity) -----

// Whether an actor is of a vanilla class ("Player", "IronGolem", "AbstractVillager", "Animal"...): its type's class
// or one it extends
bool isOfClass(Actor& actor, const std::string& javaClass);
// Entity.getVisibilityPercent: sneaking players are harder to notice (invisible ones much more)
double visibilityPercent(Actor& target, Actor* looker);

struct TargetingConditions {
	bool								isCombat		 = true;
	double								range			 = -1.0;
	bool								checkLineOfSight = true;
	bool								testInvisible	 = true;
	std::function<bool(Actor&, Level&)> selector;

	static TargetingConditions forCombat() { return {}; }
	static TargetingConditions forNonCombat() {
		TargetingConditions c;
		c.isCombat = false;
		return c;
	}
	TargetingConditions& withRange(double r) {
		range = r;
		return *this;
	}
	TargetingConditions& ignoreLineOfSight() {
		checkLineOfSight = false;
		return *this;
	}
	TargetingConditions& ignoreInvisibilityTesting() {
		testInvisible = false;
		return *this;
	}
	TargetingConditions& withSelector(std::function<bool(Actor&, Level&)> s) {
		selector = std::move(s);
		return *this;
	}
	bool test(Level& level, Mob* looker, Actor& target) const;
};

// Level.getNearestPlayer(conditions, mob, x, y, z) and getNearestEntity(candidates, ...)
Actor* nearestPlayer(Level& level, const TargetingConditions& conditions, Mob* looker, double x, double y, double z);
Actor* nearestActor(Level& level, const std::vector<Actor*>& candidates, const TargetingConditions& conditions, Mob* looker, double x, double y, double z);
// Level.getEntitiesOfClass: the actors of a class touching the box
std::vector<Actor*> actorsOfClass(Level& level, const std::string& javaClass, const AABB& box, const std::function<bool(Actor&)>& filter = nullptr);
// EntitySelector.NO_CREATIVE_OR_SPECTATOR
bool notCreativeOrSpectator(Actor& actor);
// BlockPos.findClosestMatch over withinManhattan
std::optional<BlockPos> findClosestMatch(const BlockPos& center, int horizontal, int vertical, const std::function<bool(const BlockPos&)>& test);

// ----- Movement -----

class FloatGoal : public Goal {
  public:
	explicit FloatGoal(Mob& mob);
	bool canUse() override;
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override;

  private:
	Mob& _mob;
};

class RandomStrollGoal : public Goal {
  public:
	RandomStrollGoal(Mob& mob, double speed, int interval = 120, bool checkNoActionTime = true);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override;
	void trigger() { _forceTrigger = true; }
	void setInterval(int interval) { _interval = interval; }

  protected:
	Mob&   _mob;
	double _wantedX = 0, _wantedY = 0, _wantedZ = 0;
	double _speedModifier;
	int	   _interval;
	bool   _forceTrigger = false;
	bool   _checkNoActionTime;
	virtual std::optional<Vec3> getPosition();
};

class WaterAvoidingRandomStrollGoal : public RandomStrollGoal {
  public:
	WaterAvoidingRandomStrollGoal(Mob& mob, double speed, float probability = 0.001F) : RandomStrollGoal(mob, speed), _probability(probability) {}

  protected:
	float				_probability;
	std::optional<Vec3> getPosition() override;
};

class LookAtPlayerGoal : public Goal {
  public:
	// lookAtClass: "Player" or another class
	LookAtPlayerGoal(Mob& mob, std::string lookAtClass, float distance, float probability = 0.02F, bool onlyHorizontal = false);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override { _lookAt.clear(); }
	void tick() override;

  protected:
	Mob&				_mob;
	EntityRef			_lookAt;
	float				_lookDistance;
	int					_lookTime = 0;
	float				_probability;
	bool				_onlyHorizontal;
	std::string			_lookAtClass;
	TargetingConditions _lookAtContext;
};

class RandomLookAroundGoal : public Goal {
  public:
	explicit RandomLookAroundGoal(Mob& mob);
	bool canUse() override;
	bool canContinueToUse() override { return _lookTime >= 0; }
	void start() override;
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override;

  private:
	Mob&   _mob;
	double _relX = 0, _relZ = 0;
	int	   _lookTime = 0;
};

class PanicGoal : public Goal {
  public:
	// panicTag: the damage type tag that makes it panic (#panic_causes)
	PanicGoal(Mob& mob, double speed, std::string panicTag = "minecraft:panic_causes");
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override { _isRunning = false; }
	bool isRunning() const { return _isRunning; }

  protected:
	Mob&		_mob;
	double		_speedModifier;
	double		_posX = 0, _posY = 0, _posZ = 0;
	bool		_isRunning = false;
	std::string _panicTag;
	virtual bool			shouldPanic();
	bool					findRandomPosition();
	std::optional<BlockPos> lookForWater(int distance);
};

class MeleeAttackGoal : public Goal {
  public:
	MeleeAttackGoal(Mob& mob, double speed, bool followingTargetEvenIfNotSeen);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override;
	bool requiresUpdateEveryTick() override { return true; }
	void tick() override;

  protected:
	Mob&				  _mob;
	double				  _speedModifier;
	bool				  _followingTargetEvenIfNotSeen;
	std::unique_ptr<Path> _path;
	double				  _pathedTargetX = 0, _pathedTargetY = 0, _pathedTargetZ = 0;
	int					  _ticksUntilNextPathRecalculation = 0;
	int					  _ticksUntilNextAttack			   = 0;
	int64_t				  _lastCanUseCheck				   = -100;

	virtual void checkAndPerformAttack(Actor& target);
	void		 resetAttackCooldown() { _ticksUntilNextAttack = adjustedTickDelay(20); }
	bool		 isTimeToAttack() const { return _ticksUntilNextAttack <= 0; }
	virtual bool canPerformAttack(Actor& target);
	int			 getTicksUntilNextAttack() const { return _ticksUntilNextAttack; }
	int			 getAttackInterval() { return adjustedTickDelay(20); }
};

// ZombieAttackGoal: the arms rise halfway to the next hit
class ZombieAttackGoal : public MeleeAttackGoal {
  public:
	using MeleeAttackGoal::MeleeAttackGoal;
	void start() override;
	void stop() override;
	void tick() override;

  private:
	int _raiseArmTicks = 0;
};

class LeapAtTargetGoal : public Goal {
  public:
	LeapAtTargetGoal(Mob& mob, float yd);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;

  private:
	Mob&	  _mob;
	EntityRef _target;
	float	  _yd;
};

class AvoidEntityGoal : public Goal {
  public:
	AvoidEntityGoal(Mob& mob, std::string avoidClass, float maxDist, double walkSpeed, double sprintSpeed,
					std::function<bool(Actor&)> avoidPredicate = nullptr, std::function<bool(Actor&)> predicateOnAvoidEntity = notCreativeOrSpectator);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override { _toAvoid.clear(); }
	void tick() override;

  protected:
	Mob&				  _mob;
	double				  _walkSpeedModifier, _sprintSpeedModifier;
	EntityRef			  _toAvoid;
	float				  _maxDist;
	std::unique_ptr<Path> _path;
	std::string			  _avoidClass;
	TargetingConditions	  _avoidEntityTargeting;
};

class MoveTowardsTargetGoal : public Goal {
  public:
	MoveTowardsTargetGoal(Mob& mob, double speed, float within);
	bool canUse() override;
	bool canContinueToUse() override;
	void stop() override { _target.clear(); }
	void start() override;

  private:
	Mob&	  _mob;
	EntityRef _target;
	double	  _wantedX = 0, _wantedY = 0, _wantedZ = 0, _speedModifier;
	float	  _within;
};

class RestrictSunGoal : public Goal {
  public:
	explicit RestrictSunGoal(Mob& mob) : _mob(mob) {}
	bool canUse() override;
	void start() override;
	void stop() override;

  private:
	Mob& _mob;
};

class FleeSunGoal : public Goal {
  public:
	FleeSunGoal(Mob& mob, double speed);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;

  protected:
	Mob&   _mob;
	double _wantedX = 0, _wantedY = 0, _wantedZ = 0, _speedModifier;
	bool   setWantedPos();
	std::optional<Vec3> getHidePos();
};

// TemptGoal: follows a player holding one of its foods (and stops when that player moves too fast, if it can be scared)
class TemptGoal : public Goal {
  public:
	TemptGoal(Mob& mob, double speed, std::function<bool(const ItemStack&)> items, bool canScare, double stopDistance = 2.5);
	bool canUse() override;
	bool canContinueToUse() override;
	void start() override;
	void stop() override;
	void tick() override;
	bool isRunning() const { return _isRunning; }

  protected:
	Mob&								   _mob;
	double								   _speedModifier;
	double								   _px = 0, _py = 0, _pz = 0, _pRotX = 0, _pRotY = 0;
	EntityRef							   _player;
	int									   _calmDown  = 0;
	bool								   _isRunning = false;
	std::function<bool(const ItemStack&)> _items;
	bool								   _canScare;
	double								   _stopDistance;
	TargetingConditions					   _targetingConditions;
};

// ----- Targets -----

class TargetGoal : public Goal {
  public:
	TargetGoal(Mob& mob, bool mustSee, bool mustReach = false) : _mob(mob), _mustSee(mustSee), _mustReach(mustReach) {}
	bool		canContinueToUse() override;
	void		start() override;
	void		stop() override;
	TargetGoal& setUnseenMemoryTicks(int ticks) {
		_unseenMemoryTicks = ticks;
		return *this;
	}

  protected:
	Mob&	  _mob;
	bool	  _mustSee;
	bool	  _mustReach;
	int		  _reachCache = 0, _reachCacheTime = 0, _unseenTicks = 0;
	EntityRef _targetMob;
	int		  _unseenMemoryTicks = 60;

	virtual double getFollowDistance();
	bool		   canAttack(Actor* target, const TargetingConditions& conditions);

  private:
	bool canReach(Actor& target);
};

class NearestAttackableTargetGoal : public TargetGoal {
  public:
	// targetClass: "Player", "IronGolem", "Turtle"...
	NearestAttackableTargetGoal(Mob& mob, std::string targetClass, int randomInterval, bool mustSee, bool mustReach,
								std::function<bool(Actor&, Level&)> selector = nullptr);
	NearestAttackableTargetGoal(Mob& mob, std::string targetClass, bool mustSee) : NearestAttackableTargetGoal(mob, std::move(targetClass), 10, mustSee, false) {}
	bool canUse() override;
	void start() override;
	void setTarget(Actor* target) { _target = target ? EntityRef(*target) : EntityRef(); }

  protected:
	std::string			_targetClass;
	int					_randomInterval;
	EntityRef			_target;
	TargetingConditions _targetConditions;
	virtual AABB		getTargetSearchArea(double distance);
	void				findTarget();
};

class HurtByTargetGoal : public TargetGoal {
  public:
	// toIgnoreDamage: classes whose attacks it ignores
	HurtByTargetGoal(Mob& mob, std::vector<std::string> toIgnoreDamage = {});
	bool			  canUse() override;
	void			  start() override;
	// setAlertOthers: the mobs of its class around join in (but those of these classes)
	HurtByTargetGoal& setAlertOthers(std::vector<std::string> toIgnoreAlert = {});

  protected:
	bool					 _alertSameType = false;
	int						 _timestamp		= 0;
	std::vector<std::string> _toIgnoreDamage, _toIgnoreAlert;
	void					 alertOthers();
	virtual void			 alertOther(Mob& other, Actor& target);
};

#endif
