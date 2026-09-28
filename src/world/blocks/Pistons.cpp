#include "world/blocks/Pistons.hpp"

#include "lib/JavaHashSet.hpp"
#include "world/PlaceContext.hpp"
#include "world/Shapes.hpp"
#include "world/entity/Entity.hpp"

#include <algorithm>
#include <unordered_map>

namespace {
	constexpr int TRIGGER_EXTEND = 0, TRIGGER_CONTRACT = 1, TRIGGER_DROP = 2;
	constexpr int MAX_PUSH_DEPTH = 12;

	int step(Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; }
} // namespace

PistonIds::PistonIds(const BlockContext& context) {
	piston				= context.block("minecraft:piston");
	stickyPiston		= context.block("minecraft:sticky_piston");
	pistonHead			= context.block("minecraft:piston_head");
	movingPiston		= context.block("minecraft:moving_piston");
	slime				= context.block("minecraft:slime_block");
	honey				= context.block("minecraft:honey_block");
	obsidian			= context.block("minecraft:obsidian");
	cryingObsidian		= context.block("minecraft:crying_obsidian");
	respawnAnchor		= context.block("minecraft:respawn_anchor");
	reinforcedDeepslate = context.block("minecraft:reinforced_deepslate");
	extended			= context.blocks.property("extended");
	type				= context.blocks.property("type");
	shortProperty		= context.blocks.property("short");
	normal				= context.blocks.value("normal");
	sticky				= context.blocks.value("sticky");
	waterlogged			= context.waterlogged;
	hasBlockEntity.assign(context.data.getBlockCount(), false);
	for (size_t block = 0; block < hasBlockEntity.size(); block++) hasBlockEntity[block] = context.data.isInstanceOf(static_cast<int>(block), "EntityBlock");
}

// ===== Pushability and the structure pushed =====

bool PistonBaseBlock::isPushable(Level& level, const PistonIds& ids, int state, const BlockPos& pos, Direction direction, bool allowDestroy,
								 Direction pistonFacing) {
	int maxY = level.maxY() - 1;
	if (pos.y < level.minY() || pos.y > maxY) return false;
	const BlockRegistry& blocks = level.blocks();
	if (blocks.isAir(state)) return true;
	int block = blocks.blockOf(state);
	if (block == ids.obsidian || block == ids.cryingObsidian || block == ids.respawnAnchor || block == ids.reinforcedDeepslate) return false;
	if (direction == Direction::Down && pos.y == level.minY()) return false;
	if (direction == Direction::Up && pos.y == maxY) return false;
	if (block != ids.piston && block != ids.stickyPiston) {
		if (level.gameData().getDestroyTime(state) == -1.0F) return false;
		switch (static_cast<GameData::PushReaction>(level.gameData().getStateProperties(state).pushReaction)) {
		case GameData::PushReaction::Block:
			return false;
		case GameData::PushReaction::Destroy:
			return allowDestroy;
		case GameData::PushReaction::PushOnly:
			return direction == pistonFacing;
		default:
			break;
		}
	} else if (blocks.getBool(state, ids.extended)) {
		return false;
	}
	return !ids.hasBlockEntity[block];
}

namespace {
	// PistonStructureResolver: the blocks a piston moves (at most 12, slime and honey pulling their neighbors) and
	// the ones it breaks
	class StructureResolver {
	  public:
		std::vector<BlockPos> toPush, toDestroy;
		Direction			  pushDirection;

		StructureResolver(Level& level, const PistonIds& ids, const BlockPos& pistonPos, Direction pistonDirection, bool extending)
			: _level(level), _ids(ids), _pistonPos(pistonPos), _pistonDirection(pistonDirection), _extending(extending) {
			pushDirection = extending ? pistonDirection : Directions::opposite(pistonDirection);
			_startPos	  = extending ? pistonPos.relative(pistonDirection) : pistonPos.relative(pistonDirection, 2);
		}

		bool resolve() {
			toPush.clear();
			toDestroy.clear();
			int state = _level.getBlockState(_startPos);
			if (!PistonBaseBlock::isPushable(_level, _ids, state, _startPos, pushDirection, false, _pistonDirection)) {
				if (_extending && pushReaction(state) == GameData::PushReaction::Destroy) {
					toDestroy.push_back(_startPos);
					return true;
				}
				return false;
			}
			if (!addBlockLine(_startPos, pushDirection)) return false;
			for (size_t i = 0; i < toPush.size(); i++) {
				BlockPos pos = toPush[i];
				if (isSticky(_level.getBlockState(pos)) && !addBranchingBlocks(pos)) return false;
			}
			return true;
		}

	  private:
		Level&			 _level;
		const PistonIds& _ids;
		BlockPos		 _pistonPos, _startPos;
		Direction		 _pistonDirection;
		bool			 _extending;

		GameData::PushReaction pushReaction(int state) const {
			return static_cast<GameData::PushReaction>(_level.gameData().getStateProperties(state).pushReaction);
		}
		bool isSticky(int state) const {
			int block = _level.blocks().blockOf(state);
			return block == _ids.slime || block == _ids.honey;
		}
		bool canStickToEachOther(int a, int b) const {
			int blockA = _level.blocks().blockOf(a), blockB = _level.blocks().blockOf(b);
			if (blockA == _ids.honey && blockB == _ids.slime) return false;
			if (blockA == _ids.slime && blockB == _ids.honey) return false;
			return isSticky(a) || isSticky(b);
		}
		int indexOf(const BlockPos& pos) const {
			auto it = std::find(toPush.begin(), toPush.end(), pos);
			return it == toPush.end() ? -1 : static_cast<int>(it - toPush.begin());
		}

		bool addBlockLine(const BlockPos& origin, Direction direction) {
			int state = _level.getBlockState(origin);
			if (_level.blocks().isAir(state)) return true;
			if (!PistonBaseBlock::isPushable(_level, _ids, state, origin, pushDirection, false, direction)) return true;
			if (origin == _pistonPos || indexOf(origin) >= 0) return true;
			int length = 1;
			if (length + static_cast<int>(toPush.size()) > MAX_PUSH_DEPTH) return false;
			// The sticky blocks behind it come along
			Direction back = Directions::opposite(pushDirection);
			while (isSticky(state)) {
				BlockPos behind		  = origin.relative(back, length);
				int		 previous	  = state;
				state				  = _level.getBlockState(behind);
				if (_level.blocks().isAir(state) || !canStickToEachOther(previous, state) ||
					!PistonBaseBlock::isPushable(_level, _ids, state, behind, pushDirection, false, back) || behind == _pistonPos) {
					break;
				}
				if (++length + static_cast<int>(toPush.size()) > MAX_PUSH_DEPTH) return false;
			}
			int added = 0;
			for (int i = length - 1; i >= 0; i--) {
				toPush.push_back(origin.relative(back, i));
				added++;
			}
			// Then the blocks in front
			for (int distance = 1;; distance++) {
				BlockPos pos   = origin.relative(pushDirection, distance);
				int		 index = indexOf(pos);
				if (index > -1) {
					reorderListAtCollision(added, index);
					for (int i = 0; i <= index + added; i++) {
						BlockPos at = toPush[i];
						if (isSticky(_level.getBlockState(at)) && !addBranchingBlocks(at)) return false;
					}
					return true;
				}
				state = _level.getBlockState(pos);
				if (_level.blocks().isAir(state)) return true;
				if (!PistonBaseBlock::isPushable(_level, _ids, state, pos, pushDirection, true, pushDirection) || pos == _pistonPos) return false;
				if (pushReaction(state) == GameData::PushReaction::Destroy) {
					toDestroy.push_back(pos);
					return true;
				}
				if (static_cast<int>(toPush.size()) >= MAX_PUSH_DEPTH) return false;
				toPush.push_back(pos);
				added++;
			}
		}

		void reorderListAtCollision(int added, int index) {
			std::vector<BlockPos> before(toPush.begin(), toPush.begin() + index);
			std::vector<BlockPos> last(toPush.end() - added, toPush.end());
			std::vector<BlockPos> middle(toPush.begin() + index, toPush.end() - added);
			toPush.clear();
			toPush.insert(toPush.end(), before.begin(), before.end());
			toPush.insert(toPush.end(), last.begin(), last.end());
			toPush.insert(toPush.end(), middle.begin(), middle.end());
		}

		bool addBranchingBlocks(const BlockPos& pos) {
			int state = _level.getBlockState(pos);
			for (Direction direction : Directions::ALL) {
				if (Shapes::axisOf(direction) == Shapes::axisOf(pushDirection)) continue;
				BlockPos neighbor = pos.relative(direction);
				if (canStickToEachOther(_level.getBlockState(neighbor), state) && !addBlockLine(neighbor, direction)) return false;
			}
			return true;
		}
	};
} // namespace

// ===== Piston base =====

PistonBaseBlock::PistonBaseBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, std::shared_ptr<const PistonIds> pistons,
								 bool sticky)
	: RedstoneBehavior(std::move(context), std::move(ids)), _pistons(std::move(pistons)), _sticky(sticky) {}

int PistonBaseBlock::getStateForPlacement(Level&, const PlaceContext& context) const {
	int state = blocks().defaultState(context.block);
	state	  = blocks().with(state, _context->facing, _context->directionValue(Directions::opposite(context.nearestLookingDirection())));
	return blocks().withBool(state, _pistons->extended, false);
}

void PistonBaseBlock::setPlacedBy(Level& level, const BlockPos& pos, int state) const { checkIfExtend(level, pos, state); }

void PistonBaseBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const { checkIfExtend(level, pos, state); }

void PistonBaseBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (blockOf(oldState) != blockOf(state) && !level.movingPiston(pos)) checkIfExtend(level, pos, state);
}

// Powered from any side but the front, or quasi-connected: from the block above's neighbors
bool PistonBaseBlock::getNeighborSignal(Level& level, const BlockPos& pos, Direction facing) const {
	for (Direction direction : Directions::ALL) {
		if (direction != facing && level.hasSignal(pos.relative(direction), direction)) return true;
	}
	if (level.hasSignal(pos, Direction::Down)) return true;
	BlockPos above = pos.above();
	for (Direction direction : Directions::ALL) {
		if (direction != Direction::Down && level.hasSignal(above.relative(direction), direction)) return true;
	}
	return false;
}

void PistonBaseBlock::checkIfExtend(Level& level, const BlockPos& pos, int state) const {
	Direction facing   = _context->direction(state, _context->facing);
	bool	  powered  = getNeighborSignal(level, pos, facing);
	bool	  extended = blocks().getBool(state, _pistons->extended);
	if (powered && !extended) {
		StructureResolver resolver(level, *_pistons, pos, facing, true);
		if (resolver.resolve()) level.blockEvent(pos, blockOf(state), TRIGGER_EXTEND, static_cast<int>(facing));
	} else if (!powered && extended) {
		BlockPos				   front	  = pos.relative(facing, 2);
		int						   frontState = level.getBlockState(front);
		int						   type		  = TRIGGER_CONTRACT;
		const Level::MovingPiston* moving	  = level.movingPiston(front);
		if (blockOf(frontState) == _pistons->movingPiston && _context->direction(frontState, _context->facing) == facing && moving && moving->extending &&
			(moving->progressO < 0.5F || level.getGameTime() == moving->lastTicked || level.isHandlingTick())) {
			type = TRIGGER_DROP;
		}
		level.blockEvent(pos, blockOf(state), type, static_cast<int>(facing));
	}
}

bool PistonBaseBlock::triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const {
	Direction facing	 = _context->direction(state, _context->facing);
	int		  extended	 = blocks().withBool(state, _pistons->extended, true);
	bool	  powered	 = getNeighborSignal(level, pos, facing);
	int		  pistonType = _sticky ? _pistons->sticky : _pistons->normal;
	if (powered && (type == TRIGGER_CONTRACT || type == TRIGGER_DROP)) {
		level.setBlock(pos, extended, Level::UPDATE_CLIENTS);
		return false;
	}
	if (!powered && type == TRIGGER_EXTEND) return false;

	if (type == TRIGGER_EXTEND) {
		if (!moveBlocks(level, pos, facing, true)) return false;
		level.setBlock(pos, extended, Level::UPDATE_ALL | Level::UPDATE_MOVE_BY_PISTON);
		level.playSound(nullptr, pos, "minecraft:block.piston.extend", Level::SoundSource::Blocks, 0.5F, level.random().nextFloat() * 0.25F + 0.6F);
		return true;
	}

	// Retracting: the head's moving block entity ends first
	BlockPos front = pos.relative(facing);
	if (Level::MovingPiston* moving = level.movingPiston(front)) {
		Level::MovingPiston copy = *moving;
		level.finalTickMovingPiston(front, copy);
	}
	int movingState = blocks().with(blocks().defaultState(_pistons->movingPiston), _context->facing, _context->directionValue(facing));
	movingState		= blocks().with(movingState, _pistons->type, pistonType);
	level.setBlock(pos, movingState, Level::UPDATE_INVISIBLE | Level::UPDATE_KNOWN_SHAPE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
	int base = blocks().with(blocks().defaultState(blockOf(state)), _context->facing, _context->directionValue(static_cast<Direction>(data & 7)));
	level.setMovingPiston(pos, {base, facing, false, true});
	level.updateNeighborsAt(pos, _pistons->movingPiston);
	level.updateNeighbourShapes(pos, movingState, Level::UPDATE_CLIENTS);
	if (_sticky) {
		BlockPos pulled		 = pos.relative(facing, 2);
		int		 pulledState = level.getBlockState(pulled);
		bool	 dropped	 = false;
		if (blockOf(pulledState) == _pistons->movingPiston) {
			if (Level::MovingPiston* moving = level.movingPiston(pulled); moving && moving->direction == facing && moving->extending) {
				Level::MovingPiston copy = *moving;
				level.finalTickMovingPiston(pulled, copy);
				dropped = true;
			}
		}
		if (!dropped) {
			int	 pulledBlock = blockOf(pulledState);
			auto reaction	 = static_cast<GameData::PushReaction>(level.gameData().getStateProperties(pulledState).pushReaction);
			if (type != TRIGGER_CONTRACT || blocks().isAir(pulledState) ||
				!isPushable(level, *_pistons, pulledState, pulled, Directions::opposite(facing), false, facing) ||
				(reaction != GameData::PushReaction::Normal && pulledBlock != _pistons->piston && pulledBlock != _pistons->stickyPiston)) {
				level.removeBlock(front, false);
			} else {
				moveBlocks(level, pos, facing, false);
			}
		}
	} else {
		level.removeBlock(front, false);
	}
	level.playSound(nullptr, pos, "minecraft:block.piston.contract", Level::SoundSource::Blocks, 0.5F, level.random().nextFloat() * 0.15F + 0.6F);
	return true;
}

bool PistonBaseBlock::moveBlocks(Level& level, const BlockPos& pos, Direction facing, bool extending) const {
	BlockPos headPos = pos.relative(facing);
	if (!extending && blockOf(level.getBlockState(headPos)) == _pistons->pistonHead) {
		level.setBlock(headPos, _ids->air, Level::UPDATE_INVISIBLE | Level::UPDATE_KNOWN_SHAPE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
	}
	StructureResolver resolver(level, *_pistons, pos, facing, extending);
	if (!resolver.resolve()) return false;

	// The positions left behind, in a Java HashMap (its order decides the updates)
	JavaHashSet<BlockPos>				left;
	std::unordered_map<BlockPos, int> leftStates;
	const std::vector<BlockPos>&		toPush	  = resolver.toPush;
	const std::vector<BlockPos>&		toDestroy = resolver.toDestroy;
	std::vector<int>					pushedStates;
	for (const BlockPos& at : toPush) {
		int state = level.getBlockState(at);
		pushedStates.push_back(state);
		left.add(at);
		leftStates[at] = state;
	}
	std::vector<int> removed; // States gone, destroyed ones first
	Direction		 moveDirection = extending ? facing : Directions::opposite(facing);
	for (int i = static_cast<int>(toDestroy.size()) - 1; i >= 0; i--) {
		BlockPos at	   = toDestroy[i];
		int		 state = level.getBlockState(at);
		level.dropResources(state, at);
		level.setBlock(at, _ids->air, Level::UPDATE_CLIENTS | Level::UPDATE_KNOWN_SHAPE);
		removed.push_back(state);
	}
	for (int i = static_cast<int>(toPush.size()) - 1; i >= 0; i--) {
		BlockPos from  = toPush[i];
		int		 state = level.getBlockState(from);
		BlockPos to	   = from.relative(moveDirection);
		if (left.remove(to)) leftStates.erase(to);
		int moving = blocks().with(blocks().defaultState(_pistons->movingPiston), _context->facing, _context->directionValue(facing));
		level.setBlock(to, moving, Level::UPDATE_INVISIBLE | Level::UPDATE_MOVE_BY_PISTON | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
		level.setMovingPiston(to, {pushedStates[i], facing, extending, false});
		removed.push_back(state);
	}
	if (extending) {
		int type   = _sticky ? _pistons->sticky : _pistons->normal;
		int head   = blocks().with(blocks().with(blocks().defaultState(_pistons->pistonHead), _context->facing, _context->directionValue(facing)), _pistons->type, type);
		int moving = blocks().with(blocks().with(blocks().defaultState(_pistons->movingPiston), _context->facing, _context->directionValue(facing)), _pistons->type, type);
		if (left.remove(headPos)) leftStates.erase(headPos);
		level.setBlock(headPos, moving, Level::UPDATE_INVISIBLE | Level::UPDATE_MOVE_BY_PISTON | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
		level.setMovingPiston(headPos, {head, facing, true, true});
	}
	std::vector<BlockPos> leftOrder = left.values();
	constexpr int		  leaveFlags = Level::UPDATE_CLIENTS | Level::UPDATE_KNOWN_SHAPE | Level::UPDATE_MOVE_BY_PISTON; // 82
	for (const BlockPos& at : leftOrder) level.setBlock(at, _ids->air, leaveFlags);
	for (const BlockPos& at : leftOrder) {
		int state = leftStates[at];
		level.behavior(state).updateIndirectNeighbourShapes(level, at, state, Level::UPDATE_CLIENTS, Level::UPDATE_LIMIT);
		level.updateNeighbourShapes(at, _ids->air, Level::UPDATE_CLIENTS);
	}

	size_t index = 0;
	for (int i = static_cast<int>(toDestroy.size()) - 1; i >= 0; i--) {
		int		 state = removed[index++];
		BlockPos at	   = toDestroy[i];
		level.behavior(state).affectNeighborsAfterRemoval(level, at, state, false);
		level.behavior(state).updateIndirectNeighbourShapes(level, at, state, Level::UPDATE_CLIENTS, Level::UPDATE_LIMIT);
		level.updateNeighborsAt(at, blockOf(state));
	}
	for (int i = static_cast<int>(toPush.size()) - 1; i >= 0; i--) level.updateNeighborsAt(toPush[i], blockOf(removed[index++]));
	if (extending) level.updateNeighborsAt(headPos, _pistons->pistonHead);
	return true;
}

// ===== Head and moving piston =====

PistonHeadBlock::PistonHeadBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, std::shared_ptr<const PistonIds> pistons)
	: RedstoneBehavior(std::move(context), std::move(ids)), _pistons(std::move(pistons)) {}

bool PistonHeadBlock::isFittingBase(int head, int base) const {
	int expected = blocks().get(head, _pistons->type) == _pistons->normal ? _pistons->piston : _pistons->stickyPiston;
	return blockOf(base) == expected && blocks().getBool(base, _pistons->extended) &&
		   _context->direction(base, _context->facing) == _context->direction(head, _context->facing);
}

bool PistonHeadBlock::canSurvive(Level& level, const BlockPos& pos, int state) const {
	Direction facing = _context->direction(state, _context->facing);
	int		  behind = level.getBlockState(pos.relative(Directions::opposite(facing)));
	return isFittingBase(state, behind) || (blockOf(behind) == _pistons->movingPiston && _context->direction(behind, _context->facing) == facing);
}

int PistonHeadBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int) const {
	if (Directions::opposite(direction) == _context->direction(state, _context->facing) && !canSurvive(level, pos, state)) return _ids->air;
	return state;
}

void PistonHeadBlock::affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool) const {
	BlockPos base = pos.relative(Directions::opposite(_context->direction(state, _context->facing)));
	if (isFittingBase(state, level.getBlockState(base))) level.destroyBlock(base, true);
}

void PistonHeadBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool) const {
	if (canSurvive(level, pos, state)) level.neighborChanged(pos.relative(Directions::opposite(_context->direction(state, _context->facing))), sourceBlock);
}

bool MovingPistonBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player&) const {
	if (level.movingPiston(pos)) return false;
	level.removeBlock(pos, false);
	return true;
}

// ===== PistonMovingBlockEntity =====

namespace {
	struct MovingPistons {
		Level&							 level;
		std::shared_ptr<const BlockContext> context;
		std::shared_ptr<const PistonIds>	pistons;

		float extendedProgress(const Level::MovingPiston& piston, float progress) const { return piston.extending ? progress - 1.0F : 1.0F - progress; }
		Direction movementDirection(const Level::MovingPiston& piston) const {
			return piston.extending ? piston.direction : Directions::opposite(piston.direction);
		}

		// getCollisionRelatedBlockState: a retracting piston's head for its own base
		int collisionState(const Level::MovingPiston& piston) const {
			const BlockRegistry& b = context->blocks;
			int					 moved = b.blockOf(piston.movedState);
			if (piston.extending || !piston.sourcePiston || (moved != pistons->piston && moved != pistons->stickyPiston)) return piston.movedState;
			int head = b.withBool(b.defaultState(pistons->pistonHead), pistons->shortProperty, piston.progress > 0.25F);
			head	 = b.with(head, pistons->type, moved == pistons->stickyPiston ? pistons->sticky : pistons->normal);
			return b.with(head, context->facing, b.get(piston.movedState, context->facing));
		}

		// moveByPositionAndProgress
		AABB placed(const BlockPos& pos, const AABB& box, const Level::MovingPiston& piston) const {
			double offset = extendedProgress(piston, piston.progress);
			return box.move(pos.x + offset * step(piston.direction, 0), pos.y + offset * step(piston.direction, 1), pos.z + offset * step(piston.direction, 2));
		}

		// PistonMath.getMovementArea
		static AABB movementArea(const AABB& box, Direction direction, double amount) {
			double d	= amount * (Shapes::isPositive(direction) ? 1 : -1);
			double low	= std::min(d, 0.0), high = std::max(d, 0.0);
			switch (direction) {
			case Direction::West:
				return {box.minX + low, box.minY, box.minZ, box.minX + high, box.maxY, box.maxZ};
			case Direction::East:
				return {box.maxX + low, box.minY, box.minZ, box.maxX + high, box.maxY, box.maxZ};
			case Direction::Down:
				return {box.minX, box.minY + low, box.minZ, box.maxX, box.minY + high, box.maxZ};
			case Direction::North:
				return {box.minX, box.minY, box.minZ + low, box.maxX, box.maxY, box.minZ + high};
			case Direction::South:
				return {box.minX, box.minY, box.maxZ + low, box.maxX, box.maxY, box.maxZ + high};
			default:
				return {box.minX, box.maxY + low, box.minZ, box.maxX, box.maxY + high, box.maxZ};
			}
		}

		static double getMovement(const AABB& area, Direction direction, const AABB& box) {
			switch (direction) {
			case Direction::East:
				return area.maxX - box.minX;
			case Direction::West:
				return box.maxX - area.minX;
			case Direction::Down:
				return box.maxY - area.minY;
			case Direction::South:
				return area.maxZ - box.minZ;
			case Direction::North:
				return box.maxZ - area.minZ;
			default:
				return area.maxY - box.minY;
			}
		}

		// moveCollidedEntities: the entities in the way are pushed (players move by themselves on their side)
		void moveCollidedEntities(const BlockPos& pos, float progress, const Level::MovingPiston& piston) const {
			Direction						  direction = movementDirection(piston);
			double							  amount	= progress - piston.progress;
			const std::vector<GameData::Box>& shape		= level.gameData().getCollisionShape(collisionState(piston));
			if (shape.empty()) return;
			AABB bounds{shape[0].minX, shape[0].minY, shape[0].minZ, shape[0].maxX, shape[0].maxY, shape[0].maxZ};
			for (const GameData::Box& box : shape) {
				bounds = {std::min(bounds.minX, box.minX), std::min(bounds.minY, box.minY), std::min(bounds.minZ, box.minZ),
						  std::max(bounds.maxX, box.maxX), std::max(bounds.maxY, box.maxY), std::max(bounds.maxZ, box.maxZ)};
			}
			AABB moved = placed(pos, bounds, piston);
			AABB area  = movementArea(moved, direction, amount);
			area	   = {std::min(area.minX, moved.minX), std::min(area.minY, moved.minY), std::min(area.minZ, moved.minZ),
						  std::max(area.maxX, moved.maxX), std::max(area.maxY, moved.maxY), std::max(area.maxZ, moved.maxZ)};
			bool slime = context->blocks.blockOf(piston.movedState) == pistons->slime;
			for (Entity* entity : level.entities().entitiesIn(area)) {
				if (slime) {
					// Launched along the movement
					Vec3 delta = entity->deltaMovement();
					int	 axis  = Shapes::axisOf(direction);
					delta.set(axis, step(direction, axis));
					entity->setDeltaMovement(delta);
				}
				double push = 0.0;
				for (const GameData::Box& box : shape) {
					AABB boxArea = movementArea(placed(pos, {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ}, piston), direction, amount);
					AABB entityBox = entity->boundingBox();
					if (!boxArea.intersects(entityBox)) continue;
					push = std::max(push, getMovement(boxArea, direction, entityBox));
					if (push >= amount) break;
				}
				if (push <= 0.0) continue;
				push = std::min(push, amount) + 0.01;
				entity->moveByPiston({push * step(direction, 0), push * step(direction, 1), push * step(direction, 2)});
			}
		}

		// moveStuckEntities: honey carries what stands on it sideways
		void moveStuckEntities(const BlockPos& pos, float progress, const Level::MovingPiston& piston) const {
			if (context->blocks.blockOf(piston.movedState) != pistons->honey) return;
			Direction direction = movementDirection(piston);
			if (direction == Direction::Up || direction == Direction::Down) return;
			AABB   top	  = placed(pos, {0.0, 1.0, 0.0, 1.0, 1.5000010000000001, 1.0}, piston);
			double amount = progress - piston.progress;
			for (Entity* entity : level.entities().entitiesIn(top)) {
				bool above = entity->position().x >= top.minX && entity->position().x <= top.maxX && entity->position().z >= top.minZ &&
							 entity->position().z <= top.maxZ;
				if (!entity->onGround() || !above) continue;
				entity->moveByPiston({amount * step(direction, 0), 0.0, amount * step(direction, 2)});
			}
		}

		void tick(const BlockPos& pos, Level::MovingPiston& piston) const {
			piston.lastTicked = level.getGameTime();
			piston.progressO  = piston.progress;
			if (piston.progressO >= 1.0F) {
				int moved = piston.movedState;
				level.removeMovingPiston(pos);
				if (context->blocks.blockOf(level.getBlockState(pos)) != pistons->movingPiston) return;
				int state = level.updateFromNeighbourShapes(moved, pos);
				if (context->blocks.isAir(state)) {
					level.setBlock(pos, moved, Level::UPDATE_INVISIBLE | Level::UPDATE_KNOWN_SHAPE | Level::UPDATE_MOVE_BY_PISTON |
												   Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
					level.updateOrDestroy(moved, state, pos, Level::UPDATE_ALL);
				} else {
					if (context->blocks.has(state, pistons->waterlogged) && context->blocks.getBool(state, pistons->waterlogged)) {
						state = context->blocks.withBool(state, pistons->waterlogged, false);
					}
					level.setBlock(pos, state, Level::UPDATE_ALL | Level::UPDATE_MOVE_BY_PISTON);
					level.neighborChanged(pos, context->blocks.blockOf(state));
				}
				return;
			}
			float progress = piston.progress + 0.5F;
			moveCollidedEntities(pos, progress, piston);
			moveStuckEntities(pos, progress, piston);
			piston.progress = std::min(progress, 1.0F);
		}

		void finalTick(const BlockPos& pos, Level::MovingPiston& piston) const {
			if (piston.progressO >= 1.0F) return;
			bool source = piston.sourcePiston;
			int	 moved	= piston.movedState;
			level.removeMovingPiston(pos);
			if (context->blocks.blockOf(level.getBlockState(pos)) != pistons->movingPiston) return;
			int state = source ? context->defaultState("minecraft:air") : level.updateFromNeighbourShapes(moved, pos);
			level.setBlock(pos, state, Level::UPDATE_ALL);
			level.neighborChanged(pos, context->blocks.blockOf(state));
		}
	};
} // namespace

void registerMovingPistons(Level& level, std::shared_ptr<const BlockContext> context, std::shared_ptr<const PistonIds> pistons) {
	auto moving = std::make_shared<MovingPistons>(MovingPistons{level, std::move(context), std::move(pistons)});
	level.setMovingPistonTicker([moving](const BlockPos& pos, Level::MovingPiston& piston) { moving->tick(pos, piston); },
								[moving](const BlockPos& pos, Level::MovingPiston& piston) { moving->finalTick(pos, piston); });
}
