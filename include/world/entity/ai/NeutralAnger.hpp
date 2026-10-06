#ifndef NEUTRAL_ANGER_HPP
#define NEUTRAL_ANGER_HPP

#include "lib/UUID.hpp"
#include "world/entity/Actor.hpp"

class Buffer;
class Mob;

// NeutralMob's state and default methods, kept by the mobs that are neutral until provoked (iron golems, wolves,
// polar bears, zombified piglins, endermen, bees): who it is angry at, and for how long (PERSISTENT_ANGER_TIME, a
// range in seconds). The universalAnger game rule is off (its default), forgiveDeadPlayers on
class NeutralAnger {
  public:
	NeutralAnger(Mob& mob, int minSeconds, int maxSeconds) : _mob(mob), _minTicks(minSeconds * 20), _maxTicks(maxSeconds * 20) {}

	int			remainingTime() const { return _remaining; }
	void		setRemainingTime(int ticks) { _remaining = ticks; }
	bool		hasTarget() const { return _hasTarget; }
	const UUID& target() const { return _target; }
	void		setTarget(const UUID* target);
	// startPersistentAngerTimer: a time drawn in its range
	void		startTimer();
	bool		isAngry() const { return _remaining > 0; }
	// NeutralMob.isAngryAt: something it may attack, and the one it is angry at
	bool		isAngryAt(Actor& target) const;
	// NeutralMob.updatePersistentAnger, every tick: follows the mob's target, counts down (not while it targets a
	// player if keepWhileTargetingPlayer), forgets when it ends or its target mob died
	void		update(bool keepWhileTargetingPlayer);
	// NeutralMob.stopBeingAngry: no last attacker, no anger, no target
	void		stopBeingAngry();
	// NeutralMob.playerDied (forgiveDeadPlayers)
	void		playerDied(const Actor& player);

	void save(Buffer& buf) const;
	void load(Buffer& buf);

  private:
	Mob& _mob;
	int	 _minTicks, _maxTicks;
	int	 _remaining = 0;
	bool _hasTarget = false;
	UUID _target{};
};

#endif
