#include "world/entity/ai/Pathfinder.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"
#include "world/entity/DismountHelper.hpp"
#include "world/entity/Mob.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

// ===================== PathType =====================

float defaultMalus(PathType type) {
	static constexpr float MALUS[] = {-1.0F, 0.0F, 0.0F, 0.0F, 0.0F, -1.0F, 0.0F, -1.0F, -1.0F, 8.0F, 8.0F, 0.0F, -1.0F,
									  8.0F,	 16.0F, 8.0F, -1.0F, 0.0F, -1.0F, -1.0F, 4.0F, -1.0F, 8.0F, 0.0F, 0.0F, 0.0F};
	return MALUS[static_cast<int>(type)];
}

// ===================== Node =====================

float Node::distanceTo(const Node& o) const {
	float dx = static_cast<float>(o.x - x), dy = static_cast<float>(o.y - y), dz = static_cast<float>(o.z - z);
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}
float Node::distanceTo(const BlockPos& o) const {
	float dx = static_cast<float>(o.x - x), dy = static_cast<float>(o.y - y), dz = static_cast<float>(o.z - z);
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}
float Node::distanceToSqr(const Node& o) const {
	float dx = static_cast<float>(o.x - x), dy = static_cast<float>(o.y - y), dz = static_cast<float>(o.z - z);
	return dx * dx + dy * dy + dz * dz;
}
float Node::distanceToXZ(const Node& o) const {
	float dx = static_cast<float>(o.x - x), dz = static_cast<float>(o.z - z);
	return std::sqrt(dx * dx + dz * dz);
}
float Node::distanceManhattan(const Node& o) const {
	return static_cast<float>(std::abs(o.x - x)) + static_cast<float>(std::abs(o.y - y)) + static_cast<float>(std::abs(o.z - z));
}
float Node::distanceManhattan(const BlockPos& o) const {
	return static_cast<float>(std::abs(o.x - x)) + static_cast<float>(std::abs(o.y - y)) + static_cast<float>(std::abs(o.z - z));
}

// ===================== BinaryHeap =====================

void BinaryHeap::insert(Node* node) {
	if (_size == static_cast<int>(_heap.size())) _heap.resize(_heap.size() * 2, nullptr);
	_heap[_size] = node;
	node->heapIdx = _size;
	upHeap(_size++);
}

Node* BinaryHeap::pop() {
	Node* top	 = _heap[0];
	_heap[0]	 = _heap[--_size];
	_heap[_size] = nullptr;
	if (_size > 0) downHeap(0);
	top->heapIdx = -1;
	return top;
}

void BinaryHeap::changeCost(Node* node, float cost) {
	float old = node->f;
	node->f	  = cost;
	if (cost < old) {
		upHeap(node->heapIdx);
	} else {
		downHeap(node->heapIdx);
	}
}

void BinaryHeap::upHeap(int index) {
	Node* node = _heap[index];
	float f	   = node->f;
	while (index > 0) {
		int	  parent = (index - 1) >> 1;
		Node* above	 = _heap[parent];
		if (!(f < above->f)) break;
		_heap[index]	= above;
		above->heapIdx = index;
		index		   = parent;
	}
	_heap[index]  = node;
	node->heapIdx = index;
}

void BinaryHeap::downHeap(int index) {
	Node* node = _heap[index];
	float f	   = node->f;
	while (true) {
		int left = 1 + (index << 1), right = left + 1;
		if (left >= _size) break;
		Node* leftNode	= _heap[left];
		float leftF		= leftNode->f;
		Node* rightNode = right >= _size ? nullptr : _heap[right];
		float rightF	= rightNode ? rightNode->f : std::numeric_limits<float>::infinity();
		if (leftF < rightF) {
			if (!(leftF < f)) break;
			_heap[index]	  = leftNode;
			leftNode->heapIdx = index;
			index			  = left;
		} else {
			if (!(rightF < f)) break;
			_heap[index]	   = rightNode;
			rightNode->heapIdx = index;
			index			   = right;
		}
	}
	_heap[index]  = node;
	node->heapIdx = index;
}

// ===================== Path =====================

Path::Path(std::vector<PathNode> nodes, const BlockPos& target, bool reached) : _nodes(std::move(nodes)), _target(target), _reached(reached) {
	if (_nodes.empty()) {
		_distToTarget = std::numeric_limits<float>::max();
	} else {
		const PathNode& end = _nodes.back();
		_distToTarget = static_cast<float>(std::abs(target.x - end.x)) + static_cast<float>(std::abs(target.y - end.y)) + static_cast<float>(std::abs(target.z - end.z));
	}
}

Vec3 Path::getEntityPosAtNode(float bbWidth, int index) const {
	const PathNode& node   = _nodes[index];
	double			offset = static_cast<int>(bbWidth + 1.0F) * 0.5;
	return {node.x + offset, static_cast<double>(node.y), node.z + offset};
}

// ===================== PathTypeCache =====================

namespace {
	// HashCommon.mix (fastutil)
	int index(int64_t key) {
		uint64_t h = static_cast<uint64_t>(key) * 0x9E3779B97F4A7C15ULL;
		h ^= h >> 32;
		h ^= h >> 16;
		return static_cast<int>(h & 4095);
	}
} // namespace

PathType PathTypeCache::getOrCompute(Level& level, const BlockPos& pos) {
	int64_t key = pos.asLong();
	int		i	= index(key);
	if (_types[i] != 0 && _positions[i] == key) return static_cast<PathType>(_types[i] - 1);
	PathType type = WalkNodeEvaluator::getPathTypeFromState(level, pos);
	_positions[i] = key;
	_types[i]	  = static_cast<uint8_t>(static_cast<int>(type) + 1);
	return type;
}

void PathTypeCache::invalidate(const BlockPos& pos) {
	int64_t key = pos.asLong();
	int		i	= index(key);
	if (_positions[i] == key) _types[i] = 0;
}

// ===================== PathfindingContext =====================

PathfindingContext::PathfindingContext(Level& level, Mob& mob) : _level(level), _mobPosition(mob.blockPosition()) {}

PathType PathfindingContext::getPathTypeFromState(int x, int y, int z) { return _level.pathTypeCache().getOrCompute(_level, {x, y, z}); }

int PathfindingContext::getBlockState(const BlockPos& pos) { return _level.getBlockState(pos); }

// ===================== NodeEvaluator =====================

void NodeEvaluator::prepare(Level& level, Mob& mob) {
	_context = std::make_unique<PathfindingContext>(level, mob);
	_mob	 = &mob;
	_nodes.clear();
	_entityWidth  = Mth::floor(mob.width() + 1.0F);
	_entityHeight = Mth::floor(mob.height() + 1.0F);
	_entityDepth  = Mth::floor(mob.width() + 1.0F);
}

void NodeEvaluator::done() {
	_context.reset();
	_mob = nullptr;
}

Node* NodeEvaluator::getNode(int x, int y, int z) {
	int	  hash = Node::createHash(x, y, z);
	auto& slot = _nodes[hash];
	if (!slot) slot = std::make_unique<Node>(x, y, z);
	return slot.get();
}

Target NodeEvaluator::getTargetNodeAt(double x, double y, double z) { return Target(*getNode(Mth::floor(x), Mth::floor(y), Mth::floor(z))); }

PathType NodeEvaluator::getPathType(Mob& mob, const BlockPos& pos) {
	PathfindingContext context(mob.level(), mob);
	return getPathType(context, pos.x, pos.y, pos.z);
}

bool NodeEvaluator::isBurningBlock(Level& level, int state) { return DismountHelper::isBurningBlock(level, state); }

// ===================== PathFinder =====================

std::unique_ptr<Path> PathFinder::findPath(Level& level, Mob& mob, const std::vector<BlockPos>& targetPositions, float maxDistance, int reachRange,
										   float multiplier) {
	_openSet.clear();
	_evaluator->prepare(level, mob);
	Node* start = _evaluator->getStart();
	if (!start) {
		_evaluator->done();
		return nullptr;
	}
	// The targets (a map by node in vanilla: two positions of the same node are one target)
	std::vector<Target>	  targets;
	std::vector<BlockPos> positions;
	for (const BlockPos& pos : targetPositions) {
		Target target = _evaluator->getTarget(pos.x, pos.y, pos.z);
		bool   known  = std::any_of(targets.begin(), targets.end(), [&](const Target& t) { return t.samePos(target); });
		if (known) continue;
		targets.push_back(target);
		positions.push_back(pos);
	}
	auto bestH = [&](Node& node) {
		float best = std::numeric_limits<float>::max();
		for (Target& target : targets) {
			float h = node.distanceTo(target);
			target.updateBest(h, &node);
			best = std::min(h, best);
		}
		return best;
	};
	start->g = 0.0F;
	start->h = bestH(*start);
	start->f = start->h;
	_openSet.insert(start);
	int				  visited	= 0;
	std::vector<int> reached;
	int				  maxVisits = static_cast<int>(_maxVisitedNodes * multiplier);
	while (!_openSet.isEmpty()) {
		if (++visited >= maxVisits) break;
		Node* current	= _openSet.pop();
		current->closed = true;
		for (size_t i = 0; i < targets.size(); i++) {
			if (current->distanceManhattan(targets[i]) <= reachRange) {
				targets[i].reached = true;
				if (std::find(reached.begin(), reached.end(), static_cast<int>(i)) == reached.end()) reached.push_back(static_cast<int>(i));
			}
		}
		if (!reached.empty()) break;
		if (current->distanceTo(*start) >= maxDistance) continue;
		int count = _evaluator->getNeighbors(_neighbors, *current);
		for (int i = 0; i < count; i++) {
			Node* neighbor			 = _neighbors[i];
			float step				 = distance(*current, *neighbor);
			neighbor->walkedDistance = current->walkedDistance + step;
			float g					 = current->g + step + neighbor->costMalus;
			if (neighbor->walkedDistance < maxDistance && (!neighbor->inOpenSet() || g < neighbor->g)) {
				neighbor->cameFrom = current;
				neighbor->g		   = g;
				neighbor->h		   = bestH(*neighbor) * 1.5F;
				if (neighbor->inOpenSet()) {
					_openSet.changeCost(neighbor, neighbor->g + neighbor->h);
				} else {
					neighbor->f = neighbor->g + neighbor->h;
					_openSet.insert(neighbor);
				}
			}
		}
	}
	// reconstructPath for each reached target (or each target when none was), the best one
	auto reconstruct = [](Node* end, const BlockPos& target, bool canReach) {
		std::vector<Path::PathNode> nodes;
		for (Node* node = end; node; node = node->cameFrom) nodes.push_back({node->x, node->y, node->z, node->type});
		std::reverse(nodes.begin(), nodes.end());
		return std::make_unique<Path>(std::move(nodes), target, canReach);
	};
	std::unique_ptr<Path> best;
	if (!reached.empty()) {
		for (int i : reached) {
			auto path = reconstruct(targets[i].bestNode, positions[i], true);
			if (!best || path->getNodeCount() < best->getNodeCount()) best = std::move(path);
		}
	} else {
		for (size_t i = 0; i < targets.size(); i++) {
			if (!targets[i].bestNode) continue;
			auto path = reconstruct(targets[i].bestNode, positions[i], false);
			if (!best || path->getDistToTarget() < best->getDistToTarget() ||
				(path->getDistToTarget() == best->getDistToTarget() && path->getNodeCount() < best->getNodeCount())) {
				best = std::move(path);
			}
		}
	}
	_evaluator->done();
	return best;
}
