#include "world/entity/Entity.hpp"

#include "data/GameData.hpp"
#include "network/server.hpp"
#include "world/Fluids.hpp"
#include "world/Level.hpp"
#include "world/Shapes.hpp"

#include <algorithm>
#include <chrono>
#include <limits>

namespace {
	// Mth.createInsecureUUID: a version 4 UUID from the level's random
	UUID randomUUID(JavaRandom& random) {
		uint64_t most  = (static_cast<uint64_t>(random.nextLong()) & 0xFFFFFFFFFFFF0FFFULL) | 0x4000ULL;
		uint64_t least = (static_cast<uint64_t>(random.nextLong()) & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
		return UUID(most, least);
	}

	bool hasLargeShape(const std::vector<GameData::Box>& boxes) {
		return std::any_of(boxes.begin(), boxes.end(), [](const GameData::Box& b) {
			return b.minX < 0 || b.minY < 0 || b.minZ < 0 || b.maxX > 1 || b.maxY > 1 || b.maxZ > 1;
		});
	}

	// Collision boxes of the blocks around `query` (vanilla's BlockCollisions): the positions one block further than the
	// box only count for blocks taller or wider than a block (fences, walls). calls visit(pos, box) for each box that
	// overlaps the query
	template <typename Visit> void forEachBlockCollision(Level& level, const AABB& query, Visit visit) {
		const GameData& data = level.gameData();
		int				minX = Mth::floor(query.minX - 1.0E-7) - 1, maxX = Mth::floor(query.maxX + 1.0E-7) + 1;
		int				minY = Mth::floor(query.minY - 1.0E-7) - 1, maxY = Mth::floor(query.maxY + 1.0E-7) + 1;
		int				minZ = Mth::floor(query.minZ - 1.0E-7) - 1, maxZ = Mth::floor(query.maxZ + 1.0E-7) + 1;
		for (int x = minX; x <= maxX; x++) {
			for (int z = minZ; z <= maxZ; z++) {
				if (!level.hasChunkAt({x, 0, z})) continue;
				for (int y = minY; y <= maxY; y++) {
					int edges = (x == minX || x == maxX) + (y == minY || y == maxY) + (z == minZ || z == maxZ);
					if (edges == 3) continue;
					int								  state = level.getBlockState({x, y, z});
					const std::vector<GameData::Box>& boxes = data.getCollisionShape(state);
					if (boxes.empty() || (edges == 1 && !hasLargeShape(boxes)) || edges == 2) continue; // Moving pistons aren't ported yet
					for (const GameData::Box& b : boxes) {
						AABB box{x + b.minX, y + b.minY, z + b.minZ, x + b.maxX, y + b.maxY, z + b.maxZ};
						if (box.intersects(query)) visit(BlockPos{x, y, z}, box);
					}
				}
			}
		}
	}

	// VoxelShape.collide along one axis: how far the box can move before touching one of the boxes
	double collideAxis(int axis, const AABB& box, const std::vector<AABB>& boxes, double desired) {
		if (std::abs(desired) < 1.0E-7) return 0.0;
		int u = (axis + 1) % 3, v = (axis + 2) % 3;
		for (const AABB& other : boxes) {
			if (other.max(u) <= box.min(u) + 1.0E-7 || other.min(u) >= box.max(u) - 1.0E-7) continue;
			if (other.max(v) <= box.min(v) + 1.0E-7 || other.min(v) >= box.max(v) - 1.0E-7) continue;
			if (desired > 0 && other.min(axis) >= box.max(axis) - 1.0E-7) {
				desired = std::min(desired, other.min(axis) - box.max(axis));
			} else if (desired < 0 && other.max(axis) <= box.min(axis) + 1.0E-7) {
				desired = std::max(desired, other.max(axis) - box.min(axis));
			}
			if (std::abs(desired) < 1.0E-7) return 0.0;
		}
		return desired;
	}
} // namespace

Entity::Entity(Level& level, int typeId, float width, float height)
	: _level(level), _random(static_cast<int64_t>(std::chrono::steady_clock::now().time_since_epoch().count())),
	  _id(level.server().getIdManager().allocate()), _uuid(randomUUID(level.random())), _typeId(typeId), _width(width), _height(height) {}

Entity::~Entity() { _level.server().getIdManager().release(_id); }

AABB Entity::boundingBox() const {
	double half = _width / 2.0;
	return {_position.x - half, _position.y, _position.z - half, _position.x + half, _position.y + _height, _position.z + half};
}

void Entity::tick() { baseTick(); }

void Entity::baseTick() {
	updateInWaterStateAndDoFluidPushing();
	// checkBelowWorld
	if (_position.y < _level.minY() - 64) discard();
	_firstTick = false;
}

// ----- Movement -----

void Entity::move(Vec3 movement) {
	if (_noPhysics) {
		_position			 = _position + movement;
		_horizontalCollision = false;
		_verticalCollision	 = false;
		return;
	}
	Vec3   allowed = collide(movement);
	double length  = allowed.lengthSqr();
	if (length > 1.0E-7 || movement.lengthSqr() - length < 1.0E-7) _position = _position + allowed;

	bool collidedX		 = !Mth::equal(movement.x, allowed.x);
	bool collidedZ		 = !Mth::equal(movement.z, allowed.z);
	_horizontalCollision = collidedX || collidedZ;
	// The server simulates its entities (isLocalInstanceAuthoritative): always updated, even without vertical movement
	_verticalCollision = movement.y != allowed.y;
	_onGround		   = _verticalCollision && movement.y < 0.0;
	checkSupportingBlock(_onGround, allowed);
	if (_removed) return;

	if (_horizontalCollision) _delta = {collidedX ? 0.0 : _delta.x, _delta.y, collidedZ ? 0.0 : _delta.z};
	// Block.updateEntityMovementAfterFallOn: landing stops the fall, slime and beds bounce (non-living: 0.8)
	if (movement.y != allowed.y) {
		BlockPos		 on		 = getOnPos(0.2f); // getOnPosLegacy
		int				 onState = _level.getBlockState(on);
		const GameData&	 data	 = _level.gameData();
		int				 block	 = data.getBlocks().blockOf(onState);
		if (block == _level.slimeBlock()) {
			if (_delta.y < 0.0) _delta.y = -_delta.y * 0.8;
		} else if (data.isInstanceOf(block, "BedBlock")) {
			if (_delta.y < 0.0) _delta.y = -_delta.y * 0.66f * 0.8;
		} else {
			_delta.y = 0.0;
		}
	}
	float speed = blockSpeedFactor();
	_delta		= _delta.multiply(speed, 1.0, speed);
	// Entity.applyEffectsFromBlocks: pressure plates... learn it is there
	_level.checkInsideBlocks(boundingBox());
}

void Entity::moveByPiston(Vec3 movement) {
	if (movement.lengthSqr() <= 1.0E-7) return;
	int64_t time = _level.getGameTime();
	if (time != _pistonDeltasTime) {
		_pistonDeltas[0] = _pistonDeltas[1] = _pistonDeltas[2] = 0.0;
		_pistonDeltasTime = time;
	}
	// One axis only, the first one moving
	for (int axis = 0; axis < 3; axis++) {
		double amount = movement.get(axis);
		if (amount == 0.0) continue;
		double total		= std::clamp(amount + _pistonDeltas[axis], -0.51, 0.51);
		amount				= total - _pistonDeltas[axis];
		_pistonDeltas[axis] = total;
		if (std::abs(amount) <= 1.0E-5F) return;
		Vec3 limited;
		limited.set(axis, amount);
		move(limited);
		return;
	}
}

Vec3 Entity::collide(const Vec3& movement) {
	if (movement.lengthSqr() == 0.0) return movement;
	AABB			  box = boundingBox();
	std::vector<AABB> boxes;
	forEachBlockCollision(_level, box.expandTowards(movement), [&](const BlockPos&, const AABB& b) { boxes.push_back(b); });
	if (boxes.empty()) return movement;
	// Direction.axisStepOrder: y first, then the larger horizontal axis
	int	 order[3] = {1, 0, 2};
	if (std::abs(movement.x) < std::abs(movement.z)) order[1] = 2, order[2] = 0;
	Vec3 result;
	for (int axis : order) {
		double wanted = movement.get(axis);
		if (wanted == 0.0) continue;
		result.set(axis, collideAxis(axis, box.move(result), boxes, wanted));
	}
	return result;
}

void Entity::checkSupportingBlock(bool onGround, const Vec3& movement) {
	if (!onGround) {
		_onGroundNoBlocks = false;
		_supportingBlock.reset();
		return;
	}
	AABB					box = boundingBox();
	AABB					below{box.minX, box.minY - 1.0E-6, box.minZ, box.maxX, box.minY, box.maxZ};
	std::optional<BlockPos> found = findSupportingBlock(below);
	if (found || _onGroundNoBlocks) {
		_supportingBlock = found;
	} else {
		_supportingBlock = findSupportingBlock(below.move(-movement.x, 0.0, -movement.z));
	}
	_onGroundNoBlocks = !found.has_value();
}

std::optional<BlockPos> Entity::findSupportingBlock(const AABB& box) {
	std::optional<BlockPos> best;
	double					bestDistance = std::numeric_limits<double>::max();
	std::vector<BlockPos>	seen;
	forEachBlockCollision(_level, box, [&](const BlockPos& pos, const AABB&) {
		if (std::find(seen.begin(), seen.end(), pos) != seen.end()) return;
		seen.push_back(pos);
		double dx = pos.x + 0.5 - _position.x, dy = pos.y + 0.5 - _position.y, dz = pos.z + 0.5 - _position.z;
		double distance = dx * dx + dy * dy + dz * dz;
		// Ties go to the greater position (BlockPos.compareTo: y, then z, then x)
		auto greater = [](const BlockPos& a, const BlockPos& b) {
			if (a.y != b.y) return a.y > b.y;
			if (a.z != b.z) return a.z > b.z;
			return a.x > b.x;
		};
		if (distance < bestDistance || (distance == bestDistance && (!best || greater(pos, *best)))) {
			best		 = pos;
			bestDistance = distance;
		}
	});
	return best;
}

BlockPos Entity::getOnPos(float offset) {
	if (_supportingBlock) {
		BlockPos support = *_supportingBlock;
		if (!(offset > 1.0E-5f)) return support;
		const GameData& data  = _level.gameData();
		int				block = data.getBlocks().blockOf(_level.getBlockState(support));
		bool fence = data.isInTag("minecraft:block", "minecraft:fences", block);
		bool wall  = data.isInTag("minecraft:block", "minecraft:walls", block) || data.isInstanceOf(block, "FenceGateBlock");
		if ((offset <= 0.5 && fence) || wall) return support;
		return {support.x, Mth::floor(_position.y - offset), support.z};
	}
	return {Mth::floor(_position.x), Mth::floor(_position.y - offset), Mth::floor(_position.z)};
}

float Entity::blockSpeedFactor() {
	const GameData& data  = _level.gameData();
	int				state = _level.getBlockState(blockPosition());
	int				block = data.getBlocks().blockOf(state);
	float			speed = data.getBlockProperties(state).speedFactor;
	if (block == _level.waterBlock() || block == _level.bubbleColumnBlock()) return speed;
	return speed == 1.0f ? data.getBlockProperties(_level.getBlockState(blockPosBelowAffectingMovement())).speedFactor : speed;
}

bool Entity::noCollision(const AABB& box) {
	bool none = true;
	forEachBlockCollision(_level, box, [&](const BlockPos&, const AABB&) { none = false; });
	return none;
}

void Entity::moveTowardsClosestSpace(double x, double y, double z) {
	BlockPos  pos{Mth::floor(x), Mth::floor(y), Mth::floor(z)};
	Vec3	  inside{x - pos.x, y - pos.y, z - pos.z};
	Direction best		   = Direction::Up;
	double	  bestDistance = std::numeric_limits<double>::max();
	for (Direction direction : {Direction::North, Direction::South, Direction::West, Direction::East, Direction::Up}) {
		if (Shapes::isFullBlock(_level.gameData().getCollisionShape(_level.getBlockState(pos.relative(direction))))) continue;
		double coordinate = inside.get(Shapes::axisOf(direction));
		double distance	  = Shapes::isPositive(direction) ? 1.0 - coordinate : coordinate;
		if (distance < bestDistance) {
			bestDistance = distance;
			best		 = direction;
		}
	}
	float speed = _random.nextFloat() * 0.2f + 0.1f;
	float step	= Shapes::isPositive(best) ? 1.0f : -1.0f;
	Vec3  slower = _delta.scale(0.75);
	int	  axis	 = Shapes::axisOf(best);
	_delta		 = slower;
	_delta.set(axis, step * speed);
}

// ----- Fluids -----

bool Entity::updateInWaterStateAndDoFluidPushing() {
	_waterHeight = _lavaHeight = 0;
	_wasTouchingWater		   = updateFluidHeightAndDoFluidPushing(false, 0.014);
	bool lava				   = updateFluidHeightAndDoFluidPushing(true, _level.isUltraWarm() ? 0.007 : 0.0023333333333333335);
	return isInWater() || lava;
}

bool Entity::touchingUnloadedChunk() {
	AABB box = boundingBox().inflate(1.0, 1.0, 1.0);
	for (int x = Mth::floor(box.minX) >> 4; x <= (Mth::ceil(box.maxX) >> 4); x++) {
		for (int z = Mth::floor(box.minZ) >> 4; z <= (Mth::ceil(box.maxZ) >> 4); z++) {
			if (!_level.hasChunkAt({x * 16, 0, z * 16})) return true;
		}
	}
	return false;
}

bool Entity::updateFluidHeightAndDoFluidPushing(bool lava, double pushStrength) {
	if (touchingUnloadedChunk()) return false;
	Fluids& fluids	= _level.fluids();
	AABB	box		= boundingBox().deflate(0.001);
	double	height	= 0.0;
	bool	inside	= false;
	Vec3	flow;
	int		flowing = 0;
	for (int x = Mth::floor(box.minX); x < Mth::ceil(box.maxX); x++) {
		for (int y = Mth::floor(box.minY); y < Mth::ceil(box.maxY); y++) {
			for (int z = Mth::floor(box.minZ); z < Mth::ceil(box.maxZ); z++) {
				BlockPos   pos{x, y, z};
				FluidState fluid = _level.getFluidState(pos);
				if (lava ? !fluids.isLava(fluid.type) : !fluids.isWater(fluid.type)) continue;
				double top = y + fluids.height(fluid, pos);
				if (top < box.minY) continue;
				inside = true;
				height = std::max(top - box.minY, height);
				if (isPushedByFluid()) {
					Vec3 push = fluids.flow(pos, fluid);
					if (height < 0.4) push = push.scale(height);
					flow = flow + push;
					flowing++;
				}
			}
		}
	}
	if (flow.length() > 0.0) {
		if (flowing > 0) flow = flow.scale(1.0 / flowing);
		flow = flow.normalize().scale(pushStrength); // Not a player: normalized
		if (std::abs(_delta.x) < 0.003 && std::abs(_delta.z) < 0.003 && flow.length() < 0.0045000000000000005) {
			flow = flow.normalize().scale(0.0045000000000000005);
		}
		_delta = _delta + flow;
	}
	(lava ? _lavaHeight : _waterHeight) = height;
	return inside;
}
