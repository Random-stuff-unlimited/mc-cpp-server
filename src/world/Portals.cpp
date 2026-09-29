#include "world/Portals.hpp"

#include "PacketIds.hpp"
#include "logger.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/PoiManager.hpp"
#include "world/World.hpp"
#include "world/entity/Entity.hpp"
#include "world/entity/EntityFactory.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	constexpr int MAX_SIZE					= 21;
	constexpr int LEVEL_EVENT_PORTAL_TRAVEL = 1032; // The portal sound, to the player that went through
	constexpr int LEVEL_EVENT_END_EYE		= 1503; // An eye placed in a frame
	constexpr int LEVEL_EVENT_END_PORTAL	= 1038; // The portal opening, heard by everyone
	constexpr int PLACE_FLAGS				= Level::UPDATE_CLIENTS | Level::UPDATE_KNOWN_SHAPE; // 18

	struct Blocks {
		int obsidian, portal, frame, endPortal;
		int axisProperty, eyeProperty, facingProperty;
	};
	const Blocks& ids(const Level& level) {
		static const GameData* cached = nullptr;
		static Blocks			blocks{};
		if (cached != &level.gameData()) {
			cached				 = &level.gameData();
			const GameData& data = level.gameData();
			blocks = {data.getStaticId("minecraft:block", "minecraft:obsidian"), data.getStaticId("minecraft:block", "minecraft:nether_portal"),
					  data.getStaticId("minecraft:block", "minecraft:end_portal_frame"), data.getStaticId("minecraft:block", "minecraft:end_portal"),
					  data.getBlocks().property("axis"), data.getBlocks().property("eye"), data.getBlocks().property("facing")};
		}
		return blocks;
	}

	int axisValue(const Level& level, int axis) { return level.blocks().value(axis == 0 ? "x" : axis == 1 ? "y" : "z"); }
	// The axis of a state's horizontal "axis" property: 0 x, 2 z, -1 if none
	int axisOf(const Level& level, int state) {
		int property = ids(level).axisProperty;
		if (!level.blocks().has(state, property)) return -1;
		return level.blocks().valueName(level.blocks().get(state, property)) == "x" ? 0 : 2;
	}

	Direction positive(int axis) { return axis == 0 ? Direction::East : axis == 1 ? Direction::Up : Direction::South; }
	Direction negative(int axis) { return axis == 0 ? Direction::West : axis == 1 ? Direction::Down : Direction::North; }
	int		  coordinate(const BlockPos& pos, int axis) { return axis == 0 ? pos.x : axis == 1 ? pos.y : pos.z; }
	double	  coordinate(const Vec3& pos, int axis) { return axis == 0 ? pos.x : axis == 1 ? pos.y : pos.z; }

	bool isFrame(Level& level, const BlockPos& pos) { return level.blocks().blockOf(level.getBlockState(pos)) == ids(level).obsidian; }
	// PortalShape.isEmpty: air, fire or portal
	bool isEmpty(Level& level, int state) {
		int block = level.blocks().blockOf(state);
		return level.blocks().isAir(state) || block == ids(level).portal || level.gameData().isInTag("minecraft:block", "minecraft:fire", block);
	}

	int distanceUntilEdgeAboveFrame(Level& level, const BlockPos& pos, Direction direction) {
		for (int i = 0; i <= MAX_SIZE; i++) {
			BlockPos at	   = pos.relative(direction, i);
			int		 state = level.getBlockState(at);
			if (!isEmpty(level, state)) {
				if (level.blocks().blockOf(state) == ids(level).obsidian) return i;
				break;
			}
			if (!isFrame(level, at.below())) break;
		}
		return 0;
	}

	// The actor's size in its current pose
	void dimensions(Actor& actor, double& width, double& height) {
		AABB box = actor.boundingBox();
		width	 = box.maxX - box.minX;
		height	 = box.maxY - box.minY;
	}

	double inverseLerp(double value, double from, double to) { return (value - from) / (to - from); }

	// PortalShape.findCollisionFreePosition: the arrival point moved out of the blocks if needed (vanilla looks for the
	// closest free point of the space around it; here the closest free cell center nearby)
	Vec3 findCollisionFreePosition(const Vec3& position, Level& level, double width, double height) {
		if (width > 4.0 || height > 4.0) return position;
		auto fits = [&](const Vec3& at) {
			double half = width / 2.0;
			return !level.hasBlockCollision({at.x - half, at.y, at.z - half, at.x + half, at.y + height, at.z + half});
		};
		if (fits(position)) return position;
		Vec3   best		= position;
		double bestDist = std::numeric_limits<double>::max();
		for (double dy = 0.0; dy <= 1.0001; dy += 0.25) {
			for (double dx = -1.0; dx <= 1.0001; dx += 0.25) {
				for (double dz = -1.0; dz <= 1.0001; dz += 0.25) {
					Vec3   at{position.x + dx, position.y + dy, position.z + dz};
					double dist = (at - position).lengthSqr();
					if (dist < bestDist && fits(at)) {
						best	 = at;
						bestDist = dist;
					}
				}
			}
		}
		return best;
	}

	// NetherPortalBlock.createDimensionTransition: the same place relative to the exit portal
	Portals::Transition createDimensionTransition(Level& destination, const Portals::FoundRectangle& exit, int axis, const Vec3& relative, Actor& actor) {
		BlockPos minCorner = exit.minCorner;
		int		 exitAxis  = axisOf(destination, destination.getBlockState(minCorner));
		if (exitAxis < 0) exitAxis = 0;
		double width, height;
		dimensions(actor, width, height);
		int	   rotation = axis == exitAxis ? 0 : 90;
		double along	= width / 2.0 + (exit.axis1Size - width) * relative.x;
		double up		= (exit.axis2Size - height) * relative.y;
		double across	= 0.5 + relative.z;
		bool   onX		= exitAxis == 0;
		Vec3   position{minCorner.x + (onX ? along : across), minCorner.y + up, minCorner.z + (onX ? across : along)};
		position = findCollisionFreePosition(position, destination, width, height);
		Portals::Transition transition;
		transition.level		= &destination;
		transition.position		= position;
		transition.yRot			= static_cast<float>(rotation);
		transition.xRot			= 0.0F;
		transition.relativeYRot = transition.relativeXRot = true;
		return transition;
	}
} // namespace

namespace Portals {

	bool inPortalDimension(const Level& level) {
		return level.dimensionName() == "minecraft:overworld" || level.dimensionName() == "minecraft:the_nether";
	}

	void PortalShape::createPortalBlocks(Level& level) const {
		int state = level.blocks().with(level.blocks().defaultState(ids(level).portal), ids(level).axisProperty, axisValue(level, axis));
		for (int y = 0; y < height; y++) {
			for (int i = 0; i < width; i++) level.setBlock(bottomLeft.relative(Direction::Up, y).relative(rightDir, i), state, PLACE_FLAGS);
		}
	}

	PortalShape findAnyShape(Level& level, const BlockPos& pos, int axis) {
		PortalShape shape;
		shape.axis	   = axis;
		shape.rightDir = axis == 0 ? Direction::West : Direction::South;
		// calculateBottomLeft: down to the frame, then along the frame to its left edge
		BlockPos start = pos;
		int		 lowest = std::max(level.minY(), pos.y - MAX_SIZE);
		while (start.y > lowest && isEmpty(level, level.getBlockState(start.below()))) start = start.below();
		Direction left	= Directions::opposite(shape.rightDir);
		int		  edge	= distanceUntilEdgeAboveFrame(level, start, left) - 1;
		if (edge < 0) {
			shape.bottomLeft = pos;
			return shape;
		}
		shape.bottomLeft = start.relative(left, edge);
		int width		 = distanceUntilEdgeAboveFrame(level, shape.bottomLeft, shape.rightDir);
		if (width < 2 || width > MAX_SIZE) return shape;
		shape.width = width;
		// calculateHeight: up while both sides are frames and the inside is empty
		int portalBlocks = 0;
		int height		 = MAX_SIZE;
		for (int y = 0; y < MAX_SIZE && height == MAX_SIZE; y++) {
			BlockPos row = shape.bottomLeft.relative(Direction::Up, y);
			if (!isFrame(level, row.relative(shape.rightDir, -1)) || !isFrame(level, row.relative(shape.rightDir, width))) {
				height = y;
				break;
			}
			for (int i = 0; i < width; i++) {
				int state = level.getBlockState(row.relative(shape.rightDir, i));
				if (!isEmpty(level, state)) {
					height = y;
					break;
				}
				if (level.blocks().blockOf(state) == ids(level).portal) portalBlocks++;
			}
		}
		bool top = true;
		for (int i = 0; i < width && top; i++) top = isFrame(level, shape.bottomLeft.relative(Direction::Up, height).relative(shape.rightDir, i));
		shape.numPortalBlocks = portalBlocks;
		shape.height		  = height >= 3 && height <= MAX_SIZE && top ? height : 0;
		return shape;
	}

	std::optional<PortalShape> findEmptyPortalShape(Level& level, const BlockPos& pos, int axis) {
		for (int a : {axis, axis == 0 ? 2 : 0}) {
			PortalShape shape = findAnyShape(level, pos, a);
			if (shape.isValid() && shape.numPortalBlocks == 0) return shape;
		}
		return std::nullopt;
	}

	FoundRectangle getLargestRectangleAround(const BlockPos& pos, int axis1, int limit1, int axis2, int limit2, const std::function<bool(const BlockPos&)>& test) {
		auto limit = [&](BlockPos at, Direction direction, int max) {
			int count = 0;
			while (count < max && test(at = at.relative(direction))) count++;
			return count;
		};
		Direction neg1 = negative(axis1), pos1 = positive(axis1), neg2 = negative(axis2), pos2 = positive(axis2);
		int		  before = limit(pos, neg1, limit1), after = limit(pos, pos1, limit1);
		int		  center = before;
		struct Bounds {
			int min, max;
		};
		std::vector<Bounds> bounds(before + 1 + after);
		bounds[center] = {limit(pos, neg2, limit2), limit(pos, pos2, limit2)};
		int base	   = bounds[center].min;
		for (int i = 1; i <= before; i++) {
			const Bounds& previous = bounds[center - (i - 1)];
			BlockPos	  at	   = pos.relative(neg1, i);
			bounds[center - i]	   = {limit(at, neg2, previous.min), limit(at, pos2, previous.max)};
		}
		for (int i = 1; i <= after; i++) {
			const Bounds& previous = bounds[center + i - 1];
			BlockPos	  at	   = pos.relative(pos1, i);
			bounds[center + i]	   = {limit(at, neg2, previous.min), limit(at, pos2, previous.max)};
		}
		int				 bestStart = 0, bestRow = 0, bestWidth = 0, bestHeight = 0;
		std::vector<int> heights(bounds.size());
		for (int row = base; row >= 0; row--) {
			for (size_t i = 0; i < bounds.size(); i++) {
				int low = base - bounds[i].min, high = base + bounds[i].max;
				heights[i] = row >= low && row <= high ? high + 1 - row : 0;
			}
			// getMaxRectangleLocation: largest rectangle under the histogram
			int				 start = 0, end = 0, height = 0;
			std::vector<int> stack{0};
			for (size_t i = 1; i <= heights.size(); i++) {
				int value = i == heights.size() ? 0 : heights[i];
				while (!stack.empty()) {
					int top = heights[stack.back()];
					if (value >= top) {
						stack.push_back(static_cast<int>(i));
						break;
					}
					stack.pop_back();
					int left = stack.empty() ? 0 : stack.back() + 1;
					if (top * (static_cast<int>(i) - left) > height * (end - start)) {
						end	   = static_cast<int>(i);
						start  = left;
						height = top;
					}
				}
				if (stack.empty()) stack.push_back(static_cast<int>(i));
			}
			int width = 1 + (end - 1) - start;
			if (width * height > bestWidth * bestHeight) {
				bestStart  = start;
				bestRow	   = row;
				bestWidth  = width;
				bestHeight = height;
			}
		}
		BlockPos corner = pos.relative(pos1, bestStart - center).relative(pos2, bestRow - base);
		return {corner, bestWidth, bestHeight};
	}

	// ----- PortalForcer -----

	std::optional<BlockPos> findClosestPortalPosition(Level& level, const BlockPos& pos, bool toNether) {
		int						radius = toNether ? 16 : 128;
		std::optional<BlockPos> best;
		double					bestDistance = 0;
		for (const PoiManager::Record* record : level.poi().getInSquare(PoiManager::Type::NetherPortal, pos, radius, PoiManager::Occupancy::Any)) {
			// Loaded to check it is still a portal (ensureLoadedAndValid)
			level.loadChunkNow(record->pos.chunkX(), record->pos.chunkZ());
			if (axisOf(level, level.getBlockState(record->pos)) < 0) continue;
			double dx = record->pos.x - pos.x, dy = record->pos.y - pos.y, dz = record->pos.z - pos.z;
			double distance = dx * dx + dy * dy + dz * dz;
			if (!best || distance < bestDistance || (distance == bestDistance && record->pos.y < best->y)) {
				best		 = record->pos;
				bestDistance = distance;
			}
		}
		return best;
	}

	std::optional<FoundRectangle> createPortal(Level& level, const BlockPos& pos, int axis) {
		Direction direction = positive(axis);
		double	  bestDistance = -1.0, fallbackDistance = -1.0;
		BlockPos  best, fallback;
		int		  top = std::min(level.maxY() - 1, level.minY() + level.dimensionType().logicalHeight - 1);
		auto	  canReplace = [&](const BlockPos& at) {
			 int state = level.getBlockState(at);
			 return level.gameData().getStateProperties(state).replaceable && level.getFluidState(at).isEmpty();
		};
		auto canHostFrame = [&](const BlockPos& origin, int offset) {
			Direction side = Directions::clockWise(direction);
			for (int i = -1; i < 3; i++) {
				for (int y = -1; y < 4; y++) {
					BlockPos at = origin.offset(Directions::stepX(direction) * i + Directions::stepX(side) * offset, y,
												Directions::stepZ(direction) * i + Directions::stepZ(side) * offset);
					if (y < 0 && !level.gameData().getStateProperties(level.getBlockState(at)).solid) return false;
					if (y >= 0 && !canReplace(at)) return false;
				}
			}
			return true;
		};
		// BlockPos.spiralAround(pos, 16, EAST, SOUTH)
		Direction legs[4] = {Direction::East, Direction::South, Direction::West, Direction::North};
		BlockPos  cursor  = pos.relative(Direction::South);
		int		  leg = -1, legSize = 0, legIndex = 0;
		while (true) {
			cursor = cursor.relative(legs[(leg + 4) % 4]);
			if (legIndex >= legSize) {
				if (leg >= 4 * 16) break;
				leg++;
				legIndex = 0;
				legSize	 = leg / 2 + 1;
			}
			legIndex++;
			// Each column: from its top down to the bottom of the world
			level.loadChunkNow(cursor.chunkX(), cursor.chunkZ());
			int		 height = std::min(top, level.getHeight(Level::Heightmap::MotionBlocking, cursor.x, cursor.z));
			BlockPos column{cursor.x, 0, cursor.z};
			for (int y = height; y >= level.minY(); y--) {
				column.y = y;
				if (!canReplace(column)) continue;
				int start = y;
				while (y > level.minY() && canReplace(column.below())) {
					y--;
					column.y = y;
				}
				if (y + 4 > top) continue;
				int gap = start - y;
				if (gap > 0 && gap < 3) continue;
				column.y = y;
				if (!canHostFrame(column, 0)) continue;
				double dx = column.x - pos.x, dy = column.y - pos.y, dz = column.z - pos.z;
				double distance = dx * dx + dy * dy + dz * dz;
				if (canHostFrame(column, -1) && canHostFrame(column, 1) && (bestDistance == -1.0 || bestDistance > distance)) {
					bestDistance = distance;
					best		 = column;
				}
				if (bestDistance == -1.0 && (fallbackDistance == -1.0 || fallbackDistance > distance)) {
					fallbackDistance = distance;
					fallback		 = column;
				}
			}
		}
		if (bestDistance == -1.0 && fallbackDistance != -1.0) {
			best		 = fallback;
			bestDistance = fallbackDistance;
		}
		int obsidian = level.blocks().defaultState(ids(level).obsidian);
		if (bestDistance == -1.0) {
			// Nowhere to stand: a floating platform
			int low = std::max(level.minY() + 1, 70), high = top - 9;
			if (high < low) return std::nullopt;
			best			= {pos.x - Directions::stepX(direction), std::clamp(pos.y, low, high), pos.z - Directions::stepZ(direction)};
			Direction side	= Directions::clockWise(direction);
			for (int s = -1; s < 2; s++) {
				for (int f = 0; f < 2; f++) {
					for (int y = -1; y < 3; y++) {
						BlockPos at = best.offset(f * Directions::stepX(direction) + s * Directions::stepX(side), y,
												  f * Directions::stepZ(direction) + s * Directions::stepZ(side));
						level.setBlock(at, y < 0 ? obsidian : level.blocks().defaultState(0), Level::UPDATE_ALL);
					}
				}
			}
		}
		for (int i = -1; i < 3; i++) {
			for (int y = -1; y < 4; y++) {
				if (i == -1 || i == 2 || y == -1 || y == 3) {
					level.setBlock(best.offset(i * Directions::stepX(direction), y, i * Directions::stepZ(direction)), obsidian, Level::UPDATE_ALL);
				}
			}
		}
		int portal = level.blocks().with(level.blocks().defaultState(ids(level).portal), ids(level).axisProperty, axisValue(level, axis));
		for (int i = 0; i < 2; i++) {
			for (int y = 0; y < 3; y++) level.setBlock(best.offset(i * Directions::stepX(direction), y, i * Directions::stepZ(direction)), portal, PLACE_FLAGS);
		}
		return FoundRectangle{best, 2, 3};
	}

	void createEndPlatform(Level& level, const BlockPos& pos, bool dropBlocks) {
		int obsidian = level.blocks().defaultState(ids(level).obsidian);
		int air		 = level.blocks().defaultState(0);
		for (int dz = -2; dz <= 2; dz++) {
			for (int dx = -2; dx <= 2; dx++) {
				for (int dy = -1; dy < 3; dy++) {
					BlockPos at	   = pos.offset(dx, dy, dz);
					int		 block = dy == -1 ? obsidian : air;
					if (level.blocks().blockOf(level.getBlockState(at)) == level.blocks().blockOf(block)) continue;
					if (dropBlocks) level.destroyBlock(at, true);
					level.setBlock(at, block, Level::UPDATE_ALL);
				}
			}
		}
	}

	// ----- Destinations -----

	std::optional<Transition> netherPortalDestination(Level& level, Actor& actor, const BlockPos& entry) {
		bool   toNether	   = level.dimensionName() != "minecraft:the_nether";
		Level* destination = level.server().getLevel(toNether ? "minecraft:the_nether" : "minecraft:overworld");
		if (!destination) return std::nullopt;
		// DimensionType.getTeleportationScale, clamped to the world border
		double scale = level.dimensionType().coordinateScale / destination->dimensionType().coordinateScale;
		auto   clamp = [](double v) { return std::clamp(v, -29999984.0, 29999984.0); };
		BlockPos target{Mth::floor(clamp(actor.position().x * scale)), Mth::floor(actor.position().y), Mth::floor(clamp(actor.position().z * scale))};

		FoundRectangle exit;
		if (std::optional<BlockPos> found = findClosestPortalPosition(*destination, target, toNether)) {
			int portalState = destination->getBlockState(*found);
			exit			= getLargestRectangleAround(*found, axisOf(*destination, portalState), MAX_SIZE, 1, MAX_SIZE,
														[&](const BlockPos& at) { return destination->getBlockState(at) == portalState; });
		} else {
			int axis = axisOf(level, level.getBlockState(entry));
			std::optional<FoundRectangle> created = createPortal(*destination, target, axis < 0 ? 0 : axis);
			if (!created) {
				g_logger->logGameInfo(ERROR, "Unable to create a portal, likely target out of worldborder", "Portals");
				return std::nullopt;
			}
			exit = *created;
		}
		// getDimensionTransitionFromExit: where the actor stood in the entry portal
		int	 entryState = level.getBlockState(entry);
		int	 axis		= axisOf(level, entryState);
		Vec3 relative{0.5, 0.0, 0.0};
		if (axis >= 0) {
			FoundRectangle entryRect = getLargestRectangleAround(entry, axis, MAX_SIZE, 1, MAX_SIZE, [&](const BlockPos& at) { return level.getBlockState(at) == entryState; });
			// PortalShape.getRelativePosition
			double width, height;
			dimensions(actor, width, height);
			double spanAlong = entryRect.axis1Size - width, spanUp = entryRect.axis2Size - height;
			double along = spanAlong > 0.0
								   ? std::clamp(inverseLerp(coordinate(actor.position(), axis) - (coordinate(entryRect.minCorner, axis) + width / 2.0), 0.0, spanAlong), 0.0, 1.0)
								   : 0.5;
			double up	 = spanUp > 0.0 ? std::clamp(inverseLerp(actor.position().y - entryRect.minCorner.y, 0.0, spanUp), 0.0, 1.0) : 0.0;
			int	   other = axis == 0 ? 2 : 0;
			double across = coordinate(actor.position(), other) - (coordinate(entryRect.minCorner, other) + 0.5);
			relative	  = {along, up, across};
			if (actor.asLiving() || actor.isPlayer()) relative.z = 0.0; // LivingEntity.resetForwardDirectionOfRelativePortalPosition
		} else {
			axis = 0;
		}
		return createDimensionTransition(*destination, exit, axis, relative, actor);
	}

	std::optional<Transition> endPortalDestination(Level& level, Actor& actor, const BlockPos&) {
		bool fromEnd = level.dimensionName() == "minecraft:the_end";
		if (fromEnd) {
			// Back to the world spawn (a player: to its bed or anchor, like a respawn)
			Level&				overworld = level.server().getLevel();
			const World::Spawn& spawn	  = overworld.world().getSpawn();
			Transition			transition;
			transition.level	= &overworld;
			transition.position = {spawn.x, spawn.y, spawn.z};
			if (Player* player = actor.asPlayer()) {
				if (Level* homeLevel = player->spawn().valid ? level.server().getLevel(player->spawn().dimension) : nullptr) {
					if (std::optional<Combat::RespawnPos> home = Combat::findRespawnAndUseSpawnBlock(*homeLevel, player->spawn(), false)) {
						transition.level	= homeLevel;
						transition.position = home->position;
						transition.yRot		= home->yaw;
						transition.xRot		= home->pitch;
						transition.portalSound = false;
						return transition;
					}
					transition.missingRespawnBlock = true;
				}
				transition.portalSound = false;
			}
			return transition;
		}
		Level* end = level.server().getLevel("minecraft:the_end");
		if (!end) return std::nullopt;
		Vec3 arrival{END_SPAWN_POINT.x + 0.5, static_cast<double>(END_SPAWN_POINT.y), END_SPAWN_POINT.z + 0.5};
		end->loadChunkNow(END_SPAWN_POINT.chunkX(), END_SPAWN_POINT.chunkZ());
		createEndPlatform(*end, BlockPos{Mth::floor(arrival.x), Mth::floor(arrival.y), Mth::floor(arrival.z)}.below(), true);
		Transition transition;
		transition.level		= end;
		transition.position		= arrival;
		transition.yRot			= Directions::toYRot(Direction::West);
		transition.xRot			= 0.0F;
		transition.relativeXRot = true;
		if (actor.isPlayer()) transition.position.y -= 1.0;
		return transition;
	}

	// ----- Going through -----

	bool canUsePortal(Actor& actor) { return actor.isAlive(); }

	void setAsInsidePortal(Actor& actor, Kind kind, const BlockPos& pos) {
		Actor::PortalState& state = actor.portal;
		if (state.cooldown > 0) {
			state.cooldown = actor.dimensionChangingDelay(); // Still on cooldown: it starts over
			return;
		}
		if (!state.active || state.kind != static_cast<int>(kind)) {
			state.active		 = true;
			state.kind			 = static_cast<int>(kind);
			state.entry			 = pos;
			state.portalTime	 = 0;
			state.insideThisTick = true;
		} else if (!state.insideThisTick) {
			state.entry			 = pos;
			state.insideThisTick = true;
		}
	}

	namespace {
		// Portal.getPortalTransitionTime: players wait 80 ticks in a nether portal (0 when invulnerable), the rest
		// goes at once
		int transitionTime(Actor& actor, Kind kind) {
			if (kind != Kind::Nether) return 0;
			if (Player* player = actor.asPlayer()) return player->isCreative() || player->isSpectator() ? 0 : 80;
			return 0;
		}
	} // namespace

	void handlePortal(Level& level, Actor& actor) {
		Actor::PortalState& state = actor.portal;
		if (state.cooldown > 0) state.cooldown--; // processPortalCooldown
		if (!state.active) return;
		// PortalProcessor.processPortalTeleportation
		bool go = false;
		if (!state.insideThisTick) {
			state.portalTime = std::max(state.portalTime - 4, 0);
		} else {
			state.insideThisTick = false;
			go					 = canUsePortal(actor) && state.portalTime++ >= transitionTime(actor, static_cast<Kind>(state.kind));
		}
		if (!go) {
			if (state.portalTime <= 0) state.active = false; // hasExpired
			return;
		}
		state.cooldown = actor.dimensionChangingDelay();
		Kind					  kind		 = static_cast<Kind>(state.kind);
		std::optional<Transition> transition = kind == Kind::Nether ? netherPortalDestination(level, actor, state.entry)
																	: endPortalDestination(level, actor, state.entry);
		if (!transition || !transition->level) return;
		Level& destination = *transition->level;
		float  yRot		   = transition->relativeYRot ? actor.yRot() + transition->yRot : transition->yRot;
		if (Player* player = actor.asPlayer()) {
			float xRot = transition->relativeXRot ? player->getPitch() + transition->xRot : transition->xRot;
			if (transition->missingRespawnBlock) {
				Buffer event;
				event.writeUByte(0); // NO_RESPAWN_BLOCK_AVAILABLE
				event.writeFloat(0.0F);
				Packet::send(player->shared_from_this(), PacketId::Play::Clientbound::GAME_EVENT, event, level.server());
			}
			if (&destination == &level) {
				player->setPosition(transition->position.x, transition->position.y, transition->position.z);
				player->setRotation(yRot, xRot);
			} else {
				level.server().changeDimension(*player, destination, transition->position.x, transition->position.y, transition->position.z, yRot, xRot);
			}
			if (transition->portalSound) {
				// TeleportTransition.PLAY_PORTAL_SOUND: to the player only
				Buffer sound;
				sound.writeInt(LEVEL_EVENT_PORTAL_TRAVEL);
				sound.writePosition(0, 0, 0);
				sound.writeInt(0);
				sound.writeBool(false);
				Packet::send(player->shared_from_this(), PacketId::Play::Clientbound::LEVEL_EVENT, sound, level.server());
			}
			return;
		}
		if (Entity* entity = actor.asEntity()) {
			Transition moved = *transition;
			moved.yRot		 = yRot;
			teleportEntity(level, *entity, moved);
		}
	}

	void teleportEntity(Level& from, Entity& entity, const Transition& transition) {
		Level& destination = *transition.level;
		if (&destination == &from) {
			entity.snapTo(transition.position, transition.yRot, entity.xRot());
			entity.hasImpulse = true;
			return;
		}
		// Entity.teleportCrossDimension: a copy of the entity joins the new level, the old one is removed
		std::unique_ptr<Entity> copy = EntityFactory::copy(entity, destination);
		if (!copy) return;
		copy->snapTo(transition.position, transition.yRot, entity.xRot());
		copy->setDeltaMovement(entity.deltaMovement());
		copy->portal.cooldown = entity.portal.cooldown;
		entity.discard();
		destination.loadChunkNow(Mth::floor(transition.position.x) >> 4, Mth::floor(transition.position.z) >> 4);
		destination.addFreshEntity(std::move(copy));
	}

	bool placeEnderEye(Level& level, const BlockPos& pos) {
		const Blocks& b		 = ids(level);
		int			  state	 = level.getBlockState(pos);
		const auto&	  blocks = level.blocks();
		if (blocks.blockOf(state) != b.frame || blocks.getBool(state, b.eyeProperty)) return false;
		int filled = blocks.withBool(state, b.eyeProperty, true);
		level.setBlock(pos, filled, Level::UPDATE_CLIENTS);
		level.updateNeighbourForOutputSignal(pos, b.frame);
		level.levelEvent(nullptr, LEVEL_EVENT_END_EYE, pos, 0);
		// EndPortalFrameBlock.getOrCreatePortalShape: a ring of 12 frames with eyes, all facing the 3x3 inside
		auto frameFacing = [&](const BlockPos& at, Direction facing) {
			int s = level.getBlockState(at);
			return blocks.blockOf(s) == b.frame && blocks.getBool(s, b.eyeProperty) && level.blockContext().direction(s, b.facingProperty) == facing;
		};
		for (int minX = pos.x - 3; minX <= pos.x + 1; minX++) {
			for (int minZ = pos.z - 3; minZ <= pos.z + 1; minZ++) {
				bool ring = true;
				for (int i = 0; i < 3 && ring; i++) {
					ring = frameFacing({minX + i, pos.y, minZ - 1}, Direction::South) && frameFacing({minX + i, pos.y, minZ + 3}, Direction::North) &&
						   frameFacing({minX - 1, pos.y, minZ + i}, Direction::East) && frameFacing({minX + 3, pos.y, minZ + i}, Direction::West);
				}
				if (!ring) continue;
				int portal = blocks.defaultState(b.endPortal);
				for (int dx = 0; dx < 3; dx++) {
					for (int dz = 0; dz < 3; dz++) {
						BlockPos at{minX + dx, pos.y, minZ + dz};
						level.destroyBlock(at, true);
						level.setBlock(at, portal, Level::UPDATE_CLIENTS);
					}
				}
				level.globalLevelEvent(LEVEL_EVENT_END_PORTAL, {minX + 1, pos.y, minZ + 1}, 0);
				return true;
			}
		}
		return true;
	}

} // namespace Portals

// ===================== NetherPortalBlock =====================

NetherPortalBlock::NetherPortalBlock(std::shared_ptr<const BlockContext> context)
	: _context(std::move(context)), _axis(_context->blocks.property("axis")), _air(_context->defaultState("minecraft:air")) {}

int NetherPortalBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos&, int neighborState) const {
	int	 axis		 = _context->blocks.valueName(_context->blocks.get(state, _axis)) == "x" ? 0 : 2;
	int	 updateAxis	 = Directions::axis(direction);
	bool otherAxis	 = axis != updateAxis && updateAxis != 1;
	bool samePortal	 = _context->blocks.blockOf(neighborState) == _context->blocks.blockOf(state);
	if (!otherAxis && !samePortal && !Portals::findAnyShape(level, pos, axis).isComplete()) return _air;
	return state;
}

void NetherPortalBlock::entityInside(Level&, const BlockPos& pos, int, Actor* actor) const {
	if (actor && Portals::canUsePortal(*actor)) Portals::setAsInsidePortal(*actor, Portals::Kind::Nether, pos);
}

// Zombified piglins come out of portals in the overworld (on normal difficulty, one chance in 1000 per tick)
void NetherPortalBlock::randomTick(Level& level, const BlockPos& pos, int state) const {
	if (!level.dimensionType().natural || level.difficulty() == 0) return; // isSpawningMonsters
	if (level.random().nextInt(2000) >= level.difficulty() || !level.anyPlayerCloseEnoughForSpawning(pos)) return;
	BlockPos below = pos;
	while (_context->blocks.blockOf(level.getBlockState(below)) == _context->blocks.blockOf(state)) below = below.below();
	int type = level.gameData().getStaticId("minecraft:entity_type", "minecraft:zombified_piglin");
	// BlockState.isValidSpawn: a block with a full top face
	if (type < 0 || !_context->isFaceSturdy(level.getBlockState(below), Direction::Up)) return;
	if (Mob* mob = level.mobs().spawn(level, type, below.above(), MobRegistry::SpawnReason::Structure)) mob->portal.cooldown = mob->dimensionChangingDelay();
}

// ===================== EndPortalBlock =====================

void EndPortalBlock::entityInside(Level& level, const BlockPos& pos, int, Actor* actor) const {
	if (!actor || !Portals::canUsePortal(*actor)) return;
	Player* player = actor->asPlayer();
	if (player && level.dimensionName() == "minecraft:the_end" && !player->seenCredits()) {
		level.server().showEndCredits(*player);
		return;
	}
	Portals::setAsInsidePortal(*actor, Portals::Kind::End, pos);
}
