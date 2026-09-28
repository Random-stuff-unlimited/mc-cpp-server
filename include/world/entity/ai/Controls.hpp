#ifndef CONTROLS_HPP
#define CONTROLS_HPP

#include "world/entity/Geometry.hpp"

#include <unordered_set>

class Mob;

// The controls a mob's goals steer it with (vanilla's net.minecraft.world.entity.ai.control), ticked at the end of
// Mob.serverAiStep in vanilla's order: move, look, jump. Without goals nothing asks them for anything and the mob
// stands still, its head slowly following its body like vanilla's idle mobs.

// MoveControl: turns a wanted position into the mob's forward/strafe input (zza, xxa) and speed.
// Only WAIT is ported: MOVE_TO, STRAFE and JUMPING belong with the AI (port MoveControl.tick when adding it)
class MoveControl {
  public:
	enum class Operation { Wait, MoveTo, Strafe, Jumping };

	explicit MoveControl(Mob& mob) : _mob(mob) {}
	virtual ~MoveControl() = default;

	bool	  hasWanted() const { return _operation == Operation::MoveTo; }
	double	  speedModifier() const { return _speedModifier; }
	void	  setWantedPosition(double x, double y, double z, double speedModifier);
	void	  strafe(float forwards, float right);
	Operation operation() const { return _operation; }
	double	  wantedX() const { return _wantedX; }
	double	  wantedY() const { return _wantedY; }
	double	  wantedZ() const { return _wantedZ; }
	virtual void tick();

  protected:
	Mob&	  _mob;
	double	  _wantedX = 0, _wantedY = 0, _wantedZ = 0, _speedModifier = 0;
	float	  _strafeForwards = 0, _strafeRight = 0;
	Operation _operation = Operation::Wait;
};

// LookControl: turns the head toward what a goal wants to look at, else back toward the body (ported)
class LookControl {
  public:
	explicit LookControl(Mob& mob) : _mob(mob) {}
	virtual ~LookControl() = default;

	void setLookAt(double x, double y, double z);
	void setLookAt(double x, double y, double z, float yMaxRotSpeed, float xMaxRotAngle);
	bool isLookingAtTarget() const { return _lookAtCooldown > 0; }
	virtual void tick();

  protected:
	Mob&   _mob;
	float  _yMaxRotSpeed = 0, _xMaxRotAngle = 0;
	int	   _lookAtCooldown = 0;
	double _wantedX = 0, _wantedY = 0, _wantedZ = 0;

	virtual bool resetXRotOnTick() const { return true; }
	void		 clampHeadRotationToBody();
	// Control.rotateTowards
	static float rotateTowards(float from, float to, float maxDelta) { return from + Mth::clamp(Mth::degreesDifference(from, to), -maxDelta, maxDelta); }
};

// JumpControl: a goal asks for a jump, the mob jumps on its next aiStep (ported)
class JumpControl {
  public:
	explicit JumpControl(Mob& mob) : _mob(mob) {}
	virtual ~JumpControl() = default;
	void		 jump() { _jump = true; }
	virtual void tick();

  protected:
	Mob& _mob;
	bool _jump = false;
};

// BodyRotationControl: the body follows the head when the mob stands still, faces its movement otherwise (ported)
class BodyRotationControl {
  public:
	explicit BodyRotationControl(Mob& mob) : _mob(mob) {}
	void clientTick();

  private:
	Mob&  _mob;
	int	  _headStableTime	  = 0;
	float _lastStableYHeadRot = 0;
};

// PathNavigation (GroundPathNavigation by default): finds a path and walks it through the MoveControl. A stub: no
// path is ever found, so the mob never moves by itself. Port PathNavigation / PathFinder / WalkNodeEvaluator here
class PathNavigation {
  public:
	explicit PathNavigation(Mob& mob) : _mob(mob) {}
	virtual ~PathNavigation() = default;

	// moveTo: false when no path could be found (always, for now)
	virtual bool moveTo(double x, double y, double z, double speedModifier);
	virtual void stop() {}
	virtual bool isDone() const { return true; }
	virtual bool isInProgress() const { return false; }
	virtual void tick() {}

  protected:
	Mob& _mob;
};

// Sensing: the line of sight checks of one tick, cached until the next (hasLineOfSight comes with the AI)
class Sensing {
  public:
	void tick() {
		_seen.clear();
		_unseen.clear();
	}

  private:
	std::unordered_set<int> _seen, _unseen;
};

#endif
