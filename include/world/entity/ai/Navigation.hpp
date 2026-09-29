#ifndef NAVIGATION_HPP
#define NAVIGATION_HPP

#include "world/entity/ai/Pathfinder.hpp"

#include <memory>
#include <optional>
#include <vector>

class Actor;
class Level;
class Mob;

// PathNavigation: finds a path to a position or an entity and follows it through the mob's MoveControl, node after
// node, noticing when the mob is stuck (vanilla's net.minecraft.world.entity.ai.navigation)
class PathNavigation {
  public:
	PathNavigation(Mob& mob, Level& level) : _mob(mob), _level(level) {}
	virtual ~PathNavigation() = default;

	void updatePathfinderMaxVisitedNodes();
	void setRequiredPathLength(float length);
	void resetMaxVisitedNodesMultiplier() { _maxVisitedNodesMultiplier = 1.0F; }
	void setMaxVisitedNodesMultiplier(float multiplier) { _maxVisitedNodesMultiplier = multiplier; }
	const std::optional<BlockPos>& getTargetPos() const { return _targetPos; }
	void setSpeedModifier(double speed) { _speedModifier = speed; }
	void recomputePath();

	// createPath: to positions (reach: how close is close enough), null if none was found
	std::unique_ptr<Path> createPath(double x, double y, double z, int reach);
	virtual std::unique_ptr<Path> createPath(const BlockPos& pos, int reach);
	std::unique_ptr<Path>		  createPath(const std::vector<BlockPos>& positions, int reach);
	virtual std::unique_ptr<Path> createPath(Actor& target, int reach);

	bool moveTo(double x, double y, double z, double speed);
	bool moveTo(double x, double y, double z, int reach, double speed);
	virtual bool moveTo(Actor& target, double speed);
	bool moveTo(std::unique_ptr<Path> path, double speed);
	Path* getPath() { return _path.get(); }

	virtual void tick();
	bool		 isDone() const { return !_path || _path->isDone(); }
	bool		 isInProgress() const { return !isDone(); }
	void		 stop() { _path.reset(); }
	bool		 isStableDestination(const BlockPos& pos);
	NodeEvaluator& getNodeEvaluator();
	void		 setCanFloat(bool can) { getNodeEvaluator().setCanFloat(can); }
	bool		 canFloat() { return getNodeEvaluator().canFloat(); }
	void		 setCanOpenDoors(bool can) { getNodeEvaluator().setCanOpenDoors(can); }
	// A block changed near the path's end: recompute it (ServerLevel.sendBlockUpdated)
	bool		 shouldRecomputePath(const BlockPos& pos) const;
	float		 getMaxDistanceToWaypoint() const { return _maxDistanceToWaypoint; }
	bool		 isStuck() const { return _isStuck; }
	virtual bool canNavigateGround() const = 0;
	virtual bool canCutCorner(PathType type) const {
		return type != PathType::DangerFire && type != PathType::DangerOther && type != PathType::WalkableDoor;
	}

  protected:
	Mob&				  _mob;
	Level&				  _level;
	std::unique_ptr<Path> _path;
	double				  _speedModifier = 0;
	int					  _tick			 = 0;
	int					  _lastStuckCheck = 0;
	Vec3				  _lastStuckCheckPos;
	BlockPos			  _timeoutCachedNode;
	int64_t				  _timeoutTimer = 0, _lastTimeoutCheck = 0;
	double				  _timeoutLimit = 0;
	float				  _maxDistanceToWaypoint = 0.5F;
	bool				  _hasDelayedRecomputation = false;
	int64_t				  _timeLastRecompute	   = 0;

	PathFinder& pathFinder();
	virtual std::unique_ptr<PathFinder> createPathFinder(int maxVisitedNodes) = 0;
	virtual Vec3	getTempMobPos()	  = 0;
	virtual bool	canUpdatePath()	  = 0;
	virtual void	trimPath();
	virtual bool	canMoveDirectly(const Vec3&, const Vec3&) { return false; }
	virtual double	getGroundY(const Vec3& position);
	virtual void	followThePath();
	void			doStuckDetection(const Vec3& position);
	std::unique_ptr<Path> createPath(const std::vector<BlockPos>& positions, int regionOffset, bool offsetUpward, int reach, float maxPathLength);
	// isClearForMovementBetween: nothing solid between two points (fluids too when asked)
	static bool		isClearForMovementBetween(Mob& mob, const Vec3& from, const Vec3& to, bool fluids);

  private:
	std::optional<BlockPos>		_targetPos;
	int							_reachRange				   = 0;
	float						_maxVisitedNodesMultiplier = 1.0F;
	std::unique_ptr<PathFinder> _pathFinder;
	bool						_isStuck			= false;
	float						_requiredPathLength = 16.0F;

	float getMaxPathLength();
	bool  shouldTargetNextNodeInDirection(const Vec3& position);
	void  resetStuckTimeout();
};

// GroundPathNavigation: walking mobs
class GroundPathNavigation : public PathNavigation {
  public:
	using PathNavigation::PathNavigation;
	using PathNavigation::createPath;
	std::unique_ptr<Path> createPath(const BlockPos& pos, int reach) override;
	std::unique_ptr<Path> createPath(Actor& target, int reach) override;
	bool				  canNavigateGround() const override { return true; }
	void				  setAvoidSun(bool avoid) { _avoidSun = avoid; }
	void				  setCanWalkOverFences(bool can) { getNodeEvaluator().setCanWalkOverFences(can); }
	void				  setCanPathToTargetsBelowSurface(bool can) { _canPathToTargetsBelowSurface = can; }

  protected:
	std::unique_ptr<PathFinder> createPathFinder(int maxVisitedNodes) override;
	Vec3						getTempMobPos() override;
	bool						canUpdatePath() override;
	void						trimPath() override;
	bool						hasValidPathType(PathType type) const { return type != PathType::Water && type != PathType::Lava && type != PathType::Open; }

  private:
	bool _avoidSun = false, _canPathToTargetsBelowSurface = false;

	BlockPos findSurfacePosition(const BlockPos& pos);
	int		 getSurfaceY();
};

// WallClimberNavigation: spiders, which climb straight to where they couldn't find a path to
class WallClimberNavigation : public GroundPathNavigation {
  public:
	using GroundPathNavigation::GroundPathNavigation;
	using PathNavigation::createPath;
	std::unique_ptr<Path> createPath(const BlockPos& pos, int reach) override;
	std::unique_ptr<Path> createPath(Actor& target, int reach) override;
	bool				  moveTo(Actor& target, double speed) override;
	using PathNavigation::moveTo;
	void				  tick() override;

  private:
	std::optional<BlockPos> _pathToPosition;
};

#endif
