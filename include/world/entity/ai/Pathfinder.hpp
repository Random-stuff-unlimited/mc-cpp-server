#ifndef PATHFINDER_HPP
#define PATHFINDER_HPP

#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

class Level;
class Mob;

// Mob pathfinding, ported from vanilla's net.minecraft.world.level.pathfinder: an A* over the blocks (PathFinder with
// a BinaryHeap), each node classified by a PathType whose malus the mob decides (a zombie avoids fire, a strider
// walks on lava), neighbors found by a NodeEvaluator (walking, swimming, flying)

// PathType, with its default malus (-1: can't go there)
enum class PathType : uint8_t {
	Blocked,
	Open,
	Walkable,
	WalkableDoor,
	Trapdoor,
	PowderSnow,
	DangerPowderSnow,
	Fence,
	Lava,
	Water,
	WaterBorder,
	Rail,
	UnpassableRail,
	DangerFire,
	DamageFire,
	DangerOther,
	DamageOther,
	DoorOpen,
	DoorWoodClosed,
	DoorIronClosed,
	Breach,
	Leaves,
	StickyHoney,
	Cocoa,
	DamageCautious,
	DangerTrapdoor,
	Count
};
float defaultMalus(PathType type);

// PathComputationType
enum class PathComputationType { Land = 1, Water = 2, Air = 4 };

// Node
struct Node {
	int		 x, y, z;
	int		 hash;
	int		 heapIdx  = -1;
	float	 g = 0, h = 0, f = 0;
	Node*	 cameFrom = nullptr;
	bool	 closed	  = false;
	float	 walkedDistance = 0, costMalus = 0;
	PathType type = PathType::Blocked;

	Node(int nx, int ny, int nz) : x(nx), y(ny), z(nz), hash(createHash(nx, ny, nz)) {}
	static int createHash(int x, int y, int z) {
		return static_cast<int>((static_cast<uint32_t>(y) & 0xFF) | (static_cast<uint32_t>(x) & 32767) << 8 | (static_cast<uint32_t>(z) & 32767) << 24 |
								(x < 0 ? 0x80000000u : 0u) | (z < 0 ? 32768u : 0u));
	}
	float	 distanceTo(const Node& o) const;
	float	 distanceTo(const BlockPos& o) const;
	float	 distanceToSqr(const Node& o) const;
	float	 distanceToXZ(const Node& o) const;
	float	 distanceManhattan(const Node& o) const;
	float	 distanceManhattan(const BlockPos& o) const;
	BlockPos asBlockPos() const { return {x, y, z}; }
	bool	 inOpenSet() const { return heapIdx >= 0; }
	bool	 samePos(const Node& o) const { return x == o.x && y == o.y && z == o.z; }
};

// Target: a destination, remembering the closest node the search reached
struct Target : Node {
	float bestHeuristic = 3.4028235E38F;
	Node* bestNode		= nullptr;
	bool  reached		= false;

	explicit Target(const Node& node) : Node(node.x, node.y, node.z) {}
	void updateBest(float heuristic, Node* node) {
		if (heuristic < bestHeuristic) {
			bestHeuristic = heuristic;
			bestNode	  = node;
		}
	}
};

// BinaryHeap: the open set, ordered by f
class BinaryHeap {
  public:
	void  insert(Node* node);
	void  clear() { _size = 0; }
	Node* pop();
	void  changeCost(Node* node, float cost);
	int	  size() const { return _size; }
	bool  isEmpty() const { return _size == 0; }

  private:
	std::vector<Node*> _heap = std::vector<Node*>(128, nullptr);
	int				   _size = 0;
	void			   upHeap(int index);
	void			   downHeap(int index);
};

// Path: the nodes to walk (copies: the search's nodes are gone once it's done), and how far along the mob is
class Path {
  public:
	struct PathNode {
		int		 x, y, z;
		PathType type;
		BlockPos asBlockPos() const { return {x, y, z}; }
		bool	 operator==(const PathNode& o) const { return x == o.x && y == o.y && z == o.z; }
	};

	Path(std::vector<PathNode> nodes, const BlockPos& target, bool reached);

	void			advance() { _nextNodeIndex++; }
	bool			notStarted() const { return _nextNodeIndex <= 0; }
	bool			isDone() const { return _nextNodeIndex >= static_cast<int>(_nodes.size()); }
	const PathNode* getEndNode() const { return _nodes.empty() ? nullptr : &_nodes.back(); }
	const PathNode& getNode(int index) const { return _nodes[index]; }
	void			truncateNodes(int length) {
		   if (static_cast<int>(_nodes.size()) > length) _nodes.resize(length);
	}
	void			replaceNode(int index, const PathNode& node) { _nodes[index] = node; }
	int				getNodeCount() const { return static_cast<int>(_nodes.size()); }
	int				getNextNodeIndex() const { return _nextNodeIndex; }
	void			setNextNodeIndex(int index) { _nextNodeIndex = index; }
	// Path.getEntityPosAtNode: the node's corner, centered for the entity's width
	Vec3			getEntityPosAtNode(float bbWidth, int index) const;
	Vec3			getNextEntityPos(float bbWidth) const { return getEntityPosAtNode(bbWidth, _nextNodeIndex); }
	BlockPos		getNodePos(int index) const { return _nodes[index].asBlockPos(); }
	BlockPos		getNextNodePos() const { return _nodes[_nextNodeIndex].asBlockPos(); }
	const PathNode& getNextNode() const { return _nodes[_nextNodeIndex]; }
	const PathNode* getPreviousNode() const { return _nextNodeIndex > 0 ? &_nodes[_nextNodeIndex - 1] : nullptr; }
	bool			sameAs(const Path* other) const { return other && _nodes == other->_nodes; }
	bool			canReach() const { return _reached; }
	const BlockPos& getTarget() const { return _target; }
	float			getDistToTarget() const { return _distToTarget; }

  private:
	std::vector<PathNode> _nodes;
	int					  _nextNodeIndex = 0;
	BlockPos			  _target;
	float				  _distToTarget;
	bool				  _reached;
};

// PathTypeCache: the path type of block positions, per level (ServerLevel.getPathTypeCache), cleared as blocks change
class PathTypeCache {
  public:
	PathType getOrCompute(Level& level, const BlockPos& pos);
	void	 invalidate(const BlockPos& pos);

  private:
	std::array<int64_t, 4096>  _positions{};
	std::array<uint8_t, 4096>  _types{}; // PathType + 1, 0 = none
};

// PathfindingContext
class PathfindingContext {
  public:
	PathfindingContext(Level& level, Mob& mob);
	PathType		getPathTypeFromState(int x, int y, int z);
	int				getBlockState(const BlockPos& pos);
	Level&			level() { return _level; }
	const BlockPos& mobPosition() const { return _mobPosition; }

  private:
	Level&	 _level;
	BlockPos _mobPosition;
};

// NodeEvaluator
class NodeEvaluator {
  public:
	virtual ~NodeEvaluator() = default;
	virtual void	prepare(Level& level, Mob& mob);
	virtual void	done();
	virtual Node*	getStart()											= 0;
	virtual Target	getTarget(double x, double y, double z)				= 0;
	virtual int		getNeighbors(Node** neighbors, Node& node)			= 0;
	virtual PathType getPathTypeOfMob(PathfindingContext& context, int x, int y, int z, Mob& mob) = 0;
	virtual PathType getPathType(PathfindingContext& context, int x, int y, int z)				  = 0;
	PathType		getPathType(Mob& mob, const BlockPos& pos);

	void setCanPassDoors(bool can) { _canPassDoors = can; }
	void setCanOpenDoors(bool can) { _canOpenDoors = can; }
	void setCanFloat(bool can) { _canFloat = can; }
	void setCanWalkOverFences(bool can) { _canWalkOverFences = can; }
	bool canPassDoors() const { return _canPassDoors; }
	bool canOpenDoors() const { return _canOpenDoors; }
	bool canFloat() const { return _canFloat; }
	bool canWalkOverFences() const { return _canWalkOverFences; }
	// NodeEvaluator.isBurningBlock
	static bool isBurningBlock(Level& level, int state);
	// The node at a position, made once per search
	Node* getNode(int x, int y, int z);
	Node* getNode(const BlockPos& pos) { return getNode(pos.x, pos.y, pos.z); }

  protected:
	std::unique_ptr<PathfindingContext>				_context;
	Mob*											_mob = nullptr;
	std::unordered_map<int, std::unique_ptr<Node>>	_nodes;
	int												_entityWidth = 1, _entityHeight = 1, _entityDepth = 1;
	bool _canPassDoors = true, _canOpenDoors = false, _canFloat = false, _canWalkOverFences = false;

	Target getTargetNodeAt(double x, double y, double z);
};

// WalkNodeEvaluator: walking mobs (and the path type rules the others start from)
class WalkNodeEvaluator : public NodeEvaluator {
  public:
	void	 prepare(Level& level, Mob& mob) override;
	void	 done() override;
	Node*	 getStart() override;
	Target	 getTarget(double x, double y, double z) override { return getTargetNodeAt(x, y, z); }
	int		 getNeighbors(Node** neighbors, Node& node) override;
	PathType getPathTypeOfMob(PathfindingContext& context, int x, int y, int z, Mob& mob) override;
	PathType getPathType(PathfindingContext& context, int x, int y, int z) override;

	// WalkNodeEvaluator.getFloorLevel: the top of the block below's collision
	static double	getFloorLevel(Level& level, const BlockPos& pos);
	// getPathTypeStatic: the block's path type, open air above a dangerous block made dangerous
	static PathType getPathTypeStatic(PathfindingContext& context, int x, int y, int z);
	static PathType getPathTypeStatic(Level& level, Mob& mob, const BlockPos& pos);
	static PathType checkNeighbourBlocks(PathfindingContext& context, int x, int y, int z, PathType type);
	// getPathTypeFromState: what the block itself is
	static PathType getPathTypeFromState(Level& level, const BlockPos& pos);

  protected:
	virtual bool isAmphibious() const { return false; }
	double		 getFloorLevel(const BlockPos& pos);
	Node*		 findAcceptedNode(int x, int y, int z, int jumpSize, double nodeFloor, Direction direction, PathType fromType);
	PathType	 getCachedPathType(int x, int y, int z);
	bool		 isNeighborValid(Node* neighbor, Node& node) const;
	bool		 isDiagonalValid(Node& node, Node* a, Node* b) const;
	bool		 isDiagonalValid(Node* node) const;
	Node*		 getStartNode(const BlockPos& pos);
	bool		 canStartAt(const BlockPos& pos);

  private:
	std::unordered_map<int64_t, PathType> _pathTypesByPos;
	std::map<std::array<double, 6>, bool> _collisionCache; // By box (vanilla's Object2BooleanMap<AABB>)
	Node*								  _reusableNeighbors[4] = {};

	bool  canReachWithoutCollision(Node& node);
	bool  hasCollisions(const AABB& box);
	Node* getNodeAndUpdateCostToMax(int x, int y, int z, PathType type, float malus);
	Node* getBlockedNode(int x, int y, int z);
	Node* getClosedNode(int x, int y, int z, PathType type);
	Node* tryJumpOn(int x, int y, int z, int jumpSize, double nodeFloor, Direction direction, PathType fromType);
	Node* tryFindFirstNonWaterBelow(int x, int y, int z, Node* node);
	Node* tryFindFirstGroundNodeBelow(int x, int y, int z);
	double mobJumpHeight() const;
	std::vector<PathType> getPathTypeWithinMobBB(PathfindingContext& context, int x, int y, int z);
};

// PathFinder: A* from the mob to the closest target it can reach
class PathFinder {
  public:
	PathFinder(std::unique_ptr<NodeEvaluator> evaluator, int maxVisitedNodes) : _evaluator(std::move(evaluator)), _maxVisitedNodes(maxVisitedNodes) {}
	void		   setMaxVisitedNodes(int max) { _maxVisitedNodes = max; }
	NodeEvaluator& evaluator() { return *_evaluator; }
	// findPath: maxDistance from the start, reachRange: close enough to a target, multiplier of the node budget
	std::unique_ptr<Path> findPath(Level& level, Mob& mob, const std::vector<BlockPos>& targets, float maxDistance, int reachRange, float multiplier);

  protected:
	virtual float distance(const Node& a, const Node& b) { return a.distanceTo(b); }

  private:
	std::unique_ptr<NodeEvaluator> _evaluator;
	int							   _maxVisitedNodes;
	Node*						   _neighbors[32] = {};
	BinaryHeap					   _openSet;
};

#endif
