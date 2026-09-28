#include "world/feature/Trees.hpp"

#include "data/GameData.hpp"
#include "lib/JavaRandom.hpp"
#include "world/Level.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/entity/Geometry.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

using json = nlohmann::json;

// ----- Trunk placers -----

// ----- Trunk placers -----

void TreeFeature::setDirtAt(Placement& p, const BlockPos& pos) const {
	int							   state	= p.level.getBlockState(pos);
	bool						   isDirt	= _context.inTag(_ids.dirt, state) && !_context.is(state, _ids.grass) && !_context.is(state, _ids.mycelium);
	// Through the trunk's setter: the dirt counts as trunk afterwards
	if (_forceDirt || !isDirt) p.set(p.trunk, pos, _dirtProvider.get(p.random));
}

bool TreeFeature::placeLog(Placement& p, const BlockPos& pos, int axis) const {
	if (!validTreePos(p.level, pos)) return false;
	int state = _trunkProvider.get(p.random);
	if (axis >= 0) {
		// trySetValue(RotatedPillarBlock.AXIS, axis)
		int				 changed	  = _context.blocks.with(state, _ids.axis, _ids.axes[axis]);
		if (changed >= 0) state = changed;
	}
	p.set(p.trunk, pos, state);
	return true;
}

void TreeFeature::placeLogIfFree(Placement& p, const BlockPos& pos) const {
	if (isFree(p.level, pos)) placeLog(p, pos);
}

std::vector<TreeFeature::Attachment> TreeFeature::placeTrunk(Placement& p, int height, const BlockPos& origin) const {
	JavaRandom&				random = p.random;
	std::vector<Attachment> attachments;
	auto					randomHorizontal = [&random] { return Directions::HORIZONTAL[random.nextInt(4)]; };
	auto					step			 = [](Direction direction, int axis) { return Directions::OFFSETS[static_cast<int>(direction)][axis]; };

	switch (_trunk) {
	case TrunkType::Straight:
		setDirtAt(p, origin.below());
		for (int y = 0; y < height; y++) placeLog(p, origin.relative(Direction::Up, y));
		attachments.push_back({origin.relative(Direction::Up, height), 0, false});
		break;

	case TrunkType::Forking: {
		setDirtAt(p, origin.below());
		Direction lean	   = randomHorizontal();
		int		  leanFrom = height - random.nextInt(4) - 1;
		int		  leanLeft = 3 - random.nextInt(3);
		int		  x = origin.x, z = origin.z;
		int		  top	   = INT32_MIN;
		for (int y = 0; y < height; y++) {
			int worldY = origin.y + y;
			if (y >= leanFrom && leanLeft > 0) {
				x += step(lean, 0);
				z += step(lean, 2);
				leanLeft--;
			}
			if (placeLog(p, {x, worldY, z})) top = worldY + 1;
		}
		if (top != INT32_MIN) attachments.push_back({{x, top, z}, 1, false});
		x				 = origin.x;
		z				 = origin.z;
		Direction branch = randomHorizontal();
		if (branch != lean) {
			int start = leanFrom - random.nextInt(2) - 1;
			int left  = 1 + random.nextInt(3);
			top		  = INT32_MIN;
			for (int y = start; y < height && left > 0; left--) {
				if (y >= 1) {
					int worldY = origin.y + y;
					x += step(branch, 0);
					z += step(branch, 2);
					if (placeLog(p, {x, worldY, z})) top = worldY + 1;
				}
				y++;
			}
			if (top != INT32_MIN) attachments.push_back({{x, top, z}, 0, false});
		}
		break;
	}

	case TrunkType::Giant:
	case TrunkType::MegaJungle: {
		BlockPos below = origin.below();
		setDirtAt(p, below);
		setDirtAt(p, below.east());
		setDirtAt(p, below.south());
		setDirtAt(p, below.south().east());
		for (int y = 0; y < height; y++) {
			placeLogIfFree(p, origin.offset(0, y, 0));
			if (y < height - 1) {
				placeLogIfFree(p, origin.offset(1, y, 0));
				placeLogIfFree(p, origin.offset(1, y, 1));
				placeLogIfFree(p, origin.offset(0, y, 1));
			}
		}
		attachments.push_back({origin.relative(Direction::Up, height), 0, true});
		if (_trunk == TrunkType::Giant) break;
		// MegaJungleTrunkPlacer: branches going out
		for (int y = height - 2 - random.nextInt(4); y > height / 2; y -= 2 + random.nextInt(4)) {
			float angle = random.nextFloat() * static_cast<float>(M_PI * 2);
			int	  x = 0, z = 0;
			for (int i = 0; i < 5; i++) {
				x = static_cast<int>(1.5F + Mth::cos(angle) * i);
				z = static_cast<int>(1.5F + Mth::sin(angle) * i);
				placeLog(p, origin.offset(x, y - 3 + i / 2, z));
			}
			attachments.push_back({origin.offset(x, y, z), -2, false});
		}
		break;
	}

	case TrunkType::DarkOak: {
		BlockPos below = origin.below();
		setDirtAt(p, below);
		setDirtAt(p, below.east());
		setDirtAt(p, below.south());
		setDirtAt(p, below.south().east());
		Direction					   lean	  = randomHorizontal();
		int							   leanFrom = height - random.nextInt(4);
		int							   leanLeft = 2 - random.nextInt(3);
		int							   x = origin.x, z = origin.z;
		int							   topY = origin.y + height - 1;
		for (int y = 0; y < height; y++) {
			if (y >= leanFrom && leanLeft > 0) {
				x += step(lean, 0);
				z += step(lean, 2);
				leanLeft--;
			}
			BlockPos pos   = {x, origin.y + y, z};
			int		 state = p.level.getBlockState(pos);
			if (_context.isAir(state) || _context.inTag(_ids.leavesTag, state)) {
				placeLog(p, pos);
				placeLog(p, pos.east());
				placeLog(p, pos.south());
				placeLog(p, pos.east().south());
			}
		}
		attachments.push_back({{x, topY, z}, 0, true});
		for (int dx = -1; dx <= 2; dx++) {
			for (int dz = -1; dz <= 2; dz++) {
				if ((dx < 0 || dx > 1 || dz < 0 || dz > 1) && random.nextInt(3) <= 0) {
					int length = random.nextInt(3) + 2;
					for (int i = 0; i < length; i++) placeLog(p, {origin.x + dx, topY - i - 1, origin.z + dz});
					attachments.push_back({{origin.x + dx, topY, origin.z + dz}, 0, false});
				}
			}
		}
		break;
	}

	case TrunkType::Fancy: {
		int treeHeight	 = height + 2;
		int trunkTop	 = javaFloor(treeHeight * 0.618);
		setDirtAt(p, origin.below());
		int clusters	 = std::min(1, javaFloor(1.382 + std::pow(1.0 * treeHeight / 13.0, 2.0)));
		int branchTop	 = origin.y + trunkTop;
		int y			 = treeHeight - 5;
		struct FoliageCoords {
			Attachment attachment;
			int		   branchBase;
		};
		std::vector<FoliageCoords> coords;
		coords.push_back({{origin.relative(Direction::Up, y), 0, false}, branchTop});
		auto treeShape = [](int size, int layer) -> float {
			if (layer < size * 0.3F) return -1.0F;
			float half	 = size / 2.0F;
			float offset = half - layer;
			float radius = static_cast<float>(std::sqrt(half * half - offset * offset));
			if (offset == 0.0F) {
				radius = half;
			} else if (std::abs(offset) >= half) {
				return 0.0F;
			}
			return radius * 0.5F;
		};
		for (; y >= 0; y--) {
			float shape = treeShape(treeHeight, y);
			if (shape < 0.0F) continue;
			for (int i = 0; i < clusters; i++) {
				double	 length = 1.0 * shape * (random.nextFloat() + 0.328);
				double	 angle	= random.nextFloat() * 2.0F * M_PI;
				double	 bx		= length * std::sin(angle) + 0.5;
				double	 bz		= length * std::cos(angle) + 0.5;
				BlockPos start	= origin.offset(javaFloor(bx), y - 1, javaFloor(bz));
				BlockPos end	= start.relative(Direction::Up, 5);
				if (!makeLimb(p, start, end, false)) continue;
				int		 dx	  = origin.x - start.x;
				int		 dz	  = origin.z - start.z;
				double	 base = start.y - std::sqrt(static_cast<double>(dx * dx + dz * dz)) * 0.381;
				int		 baseY = base > branchTop ? branchTop : static_cast<int>(base);
				BlockPos trunkPos{origin.x, baseY, origin.z};
				if (makeLimb(p, trunkPos, start, false)) coords.push_back({{start, 0, false}, trunkPos.y});
			}
		}
		makeLimb(p, origin, origin.relative(Direction::Up, trunkTop), true);
		// makeBranches
		for (const FoliageCoords& coord : coords) {
			BlockPos base{origin.x, coord.branchBase, origin.z};
			if (base != coord.attachment.pos && coord.branchBase - origin.y >= treeHeight * 0.2) makeLimb(p, base, coord.attachment.pos, true);
		}
		for (const FoliageCoords& coord : coords) {
			if (coord.branchBase - origin.y >= treeHeight * 0.2) attachments.push_back(coord.attachment);
		}
		break;
	}

	case TrunkType::Cherry: {
		setDirtAt(p, origin.below());
		int first  = std::max(0, height - 1 + _branchStart.sample(random));
		int second = std::max(0, height - 1 + _secondBranchStart.sample(random));
		if (second >= first) second++;
		int	 count = _branchCount.sample(random);
		bool three = count == 3;
		bool two   = count >= 2;
		int	 trunk = three ? height : two ? std::max(first, second) + 1 : first + 1;
		for (int y = 0; y < trunk; y++) placeLog(p, origin.relative(Direction::Up, y));
		if (three) attachments.push_back({origin.relative(Direction::Up, trunk), 0, false});
		Direction direction = randomHorizontal();
		int		  axis		= direction == Direction::East || direction == Direction::West ? 0 : 2;
		attachments.push_back(cherryBranch(p, height, origin, direction, first, first < trunk - 1, axis));
		if (two) attachments.push_back(cherryBranch(p, height, origin, Directions::opposite(direction), second, second < trunk - 1, axis));
		break;
	}
	}
	return attachments;
}

// FancyTrunkPlacer.makeLimb: a line of logs (place) or whether one fits
bool TreeFeature::makeLimb(Placement& p, const BlockPos& start, const BlockPos& end, bool place) const {
	if (!place && start == end) return true;
	int	  dx = end.x - start.x, dy = end.y - start.y, dz = end.z - start.z;
	int	  steps = std::max(std::abs(dx), std::max(std::abs(dy), std::abs(dz)));
	float sx = static_cast<float>(dx) / steps, sy = static_cast<float>(dy) / steps, sz = static_cast<float>(dz) / steps;
	for (int i = 0; i <= steps; i++) {
		BlockPos pos = start.offset(javaFloor(0.5F + i * sx), javaFloor(0.5F + i * sy), javaFloor(0.5F + i * sz));
		if (place) {
			// getLogAxis: along the longest horizontal distance from the start, y if none
			int ax = std::abs(pos.x - start.x), az = std::abs(pos.z - start.z);
			int longest = std::max(ax, az);
			placeLog(p, pos, longest > 0 ? (ax == longest ? 0 : 2) : 1);
		} else if (!isFree(p.level, pos)) {
			return false;
		}
	}
	return true;
}

TreeFeature::Attachment TreeFeature::cherryBranch(Placement& p, int height, const BlockPos& origin, Direction direction, int start, bool trunkAbove,
												  int axis) const {
	BlockPos pos	   = origin.relative(Direction::Up, start);
	int		 endY	   = height - 1 + _branchEnd.sample(p.random);
	bool	 extended  = trunkAbove || endY < start;
	int		 length	   = _branchHorizontalLength.sample(p.random) + (extended ? 1 : 0);
	BlockPos end	   = origin.relative(direction, length).relative(Direction::Up, endY);
	int		 sideways = extended ? 2 : 1;
	for (int i = 0; i < sideways; i++) {
		pos = pos.relative(direction);
		placeLog(p, pos, axis);
	}
	Direction vertical = end.y > pos.y ? Direction::Up : Direction::Down;
	while (true) {
		int distance = std::abs(pos.x - end.x) + std::abs(pos.y - end.y) + std::abs(pos.z - end.z);
		if (distance == 0) return {end.above(), 0, false};
		float chance = static_cast<float>(std::abs(end.y - pos.y)) / distance;
		bool  up	 = p.random.nextFloat() < chance;
		pos			 = pos.relative(up ? vertical : direction);
		placeLog(p, pos, up ? -1 : axis);
	}
}
