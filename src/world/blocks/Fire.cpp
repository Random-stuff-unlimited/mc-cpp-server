#include "world/blocks/Fire.hpp"

#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/Explosion.hpp"
#include "world/PlaceContext.hpp"
#include "world/Portals.hpp"
#include "world/Survival.hpp"
#include "world/entity/LivingEntity.hpp"

#include <algorithm>

namespace {
	// Level events (LevelEvent)
	constexpr int LEVEL_EVENT_FIRE_EXTINGUISH = 1009;

	int fireBlockId(const Level& level) { return level.fireBlock(); }
} // namespace

// ===================== BaseFireBlock =====================

BaseFireBlock::BaseFireBlock(std::shared_ptr<const BlockContext> context, float fireDamage)
	: _context(std::move(context)), _fireDamage(fireDamage), _air(_context->defaultState("minecraft:air")) {}

int BaseFireBlock::getState(Level& level, const BlockPos& pos) {
	int below = level.getBlockState(pos.below());
	if (SoulFireBlock::canSurviveOnBlock(level, below)) return level.blocks().defaultState(level.soulFireBlock());
	const auto& fire = static_cast<const FireBlock&>(level.behaviors().get(fireBlockId(level)));
	return fire.getStateAt(level, pos);
}

bool BaseFireBlock::canBePlacedAt(Level& level, const BlockPos& pos, Direction direction) {
	if (!level.blocks().isAir(level.getBlockState(pos))) return false;
	int state = getState(level, pos);
	if (level.behavior(state).canSurvive(level, pos, state)) return true;
	// isPortal: an obsidian block next to it, and an empty portal frame around it
	if (!Portals::inPortalDimension(level)) return false;
	int	 obsidian = level.gameData().getStaticId("minecraft:block", "minecraft:obsidian");
	bool frame	  = false;
	for (Direction d : Directions::ALL) {
		if (level.blocks().blockOf(level.getBlockState(pos.relative(d))) == obsidian) {
			frame = true;
			break;
		}
	}
	if (!frame) return false;
	// The axis of the portal: across the face clicked, a random one when clicking the top or bottom
	int axis = Directions::axis(direction) != 1 ? Directions::axis(Directions::counterClockWise(direction)) : (level.random().nextInt(2) == 0 ? 0 : 2);
	return Portals::findEmptyPortalShape(level, pos, axis).has_value();
}

void BaseFireBlock::fireIgnite(Level& level, Actor& actor) {
	if (LivingEntity* living = actor.asLiving()) {
		if (living->fireImmune()) return;
		if (living->remainingFireTicks() < 0) living->setRemainingFireTicks(living->remainingFireTicks() + 1);
		if (living->remainingFireTicks() >= 0) living->igniteForTicks(160);
	} else if (Player* player = actor.asPlayer()) {
		// A player burns a little longer each tick it stays in the fire (nextInt(1, 3))
		int ticks = player->survival().remainingFireTicks;
		if (ticks < 0) {
			ticks++;
		} else {
			ticks += level.random().nextInt(2) + 1;
		}
		player->survival().remainingFireTicks = ticks;
		if (ticks >= 0) Survival::igniteForTicks(*player, 160);
	}
}

// BaseFireBlock.entityInside: sets it on fire, then burns it (the effects applied after the other blocks' ones)
void BaseFireBlock::entityInside(Level& level, const BlockPos&, int, Actor* entity) const {
	// Living entities handle their fire in LivingEntity::applyFireEffectsFromBlocks, players in Survival: the block
	// only lights the others (items burn in ItemEntity)
	(void)level;
	(void)entity;
}

void BaseFireBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (_context->blocks.blockOf(oldState) == _context->blocks.blockOf(state)) return;
	if (Portals::inPortalDimension(level)) {
		if (std::optional<Portals::PortalShape> shape = Portals::findEmptyPortalShape(level, pos, 0)) {
			shape->createPortalBlocks(level);
			return;
		}
	}
	if (!level.behavior(state).canSurvive(level, pos, state)) level.removeBlock(pos, false);
}

void BaseFireBlock::playerWillDestroy(Level& level, const BlockPos& pos, int, Player&) const { level.levelEvent(nullptr, LEVEL_EVENT_FIRE_EXTINGUISH, pos, 0); }

// ===================== FireBlock =====================

FireBlock::FireBlock(std::shared_ptr<const BlockContext> context) : BaseFireBlock(std::move(context), 1.0F) {
	const BlockContext& c = *_context;
	size_t				count = c.data.getBlockCount();
	_igniteOdds.assign(count, 0);
	_burnOdds.assign(count, 0);
	for (const Flammability& entry : flammabilityTable()) {
		int block = c.block(entry.block);
		if (block < 0) continue;
		_igniteOdds[block] = entry.igniteOdds;
		_burnOdds[block]   = entry.burnOdds;
	}
	_age = c.blocks.property("age");
	for (int& face : _faces) face = -1;
	_faces[static_cast<int>(Direction::Up)]	   = c.blocks.property("up");
	_faces[static_cast<int>(Direction::North)] = c.blocks.property("north");
	_faces[static_cast<int>(Direction::South)] = c.blocks.property("south");
	_faces[static_cast<int>(Direction::West)]  = c.blocks.property("west");
	_faces[static_cast<int>(Direction::East)]  = c.blocks.property("east");
	_tnt									   = c.block("minecraft:tnt");
}

// A waterlogged block doesn't burn
int FireBlock::igniteOdds(int state) const {
	const BlockContext& c = *_context;
	if (c.blocks.has(state, c.waterlogged) && c.blocks.getBool(state, c.waterlogged)) return 0;
	return _igniteOdds[c.blocks.blockOf(state)];
}

int FireBlock::burnOdds(int state) const {
	const BlockContext& c = *_context;
	if (c.blocks.has(state, c.waterlogged) && c.blocks.getBool(state, c.waterlogged)) return 0;
	return _burnOdds[c.blocks.blockOf(state)];
}

int FireBlock::getStateAt(Level& level, const BlockPos& pos) const {
	const BlockContext& c	  = *_context;
	int					fire  = c.blocks.defaultState(level.fireBlock());
	BlockPos			below = pos.below();
	int					belowState = level.getBlockState(below);
	if (canBurn(belowState) || c.isFaceSturdy(belowState, Direction::Up)) return fire;
	for (Direction d : Directions::ALL) {
		int property = _faces[static_cast<int>(d)];
		if (property >= 0) fire = c.blocks.withBool(fire, property, canBurn(level.getBlockState(pos.relative(d))));
	}
	return fire;
}

int FireBlock::getStateForPlacement(Level& level, const PlaceContext& context) const { return getStateAt(level, context.clickedPos); }

int FireBlock::getStateWithAge(Level& level, const BlockPos& pos, int age) const {
	int state = getState(level, pos);
	return _context->blocks.blockOf(state) == level.fireBlock() ? _context->blocks.withInt(state, _age, age) : state;
}

int FireBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	return canSurvive(level, pos, state) ? getStateWithAge(level, pos, _context->blocks.getInt(state, _age)) : _air;
}

bool FireBlock::isValidFireLocation(Level& level, const BlockPos& pos) const {
	for (Direction d : Directions::ALL) {
		if (canBurn(level.getBlockState(pos.relative(d)))) return true;
	}
	return false;
}

bool FireBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	return _context->isFaceSturdy(level.getBlockState(pos.below()), Direction::Up) || isValidFireLocation(level, pos);
}

int FireBlock::igniteOddsAround(Level& level, const BlockPos& pos) const {
	if (!_context->blocks.isAir(level.getBlockState(pos))) return 0; // isEmptyBlock
	int odds = 0;
	for (Direction d : Directions::ALL) odds = std::max(odds, igniteOdds(level.getBlockState(pos.relative(d))));
	return odds;
}

bool FireBlock::isNearRain(Level& level, const BlockPos& pos) const {
	return level.isRainingAt(pos) || level.isRainingAt(pos.west()) || level.isRainingAt(pos.east()) || level.isRainingAt(pos.north()) ||
		   level.isRainingAt(pos.south());
}

void FireBlock::checkBurnOut(Level& level, const BlockPos& pos, int chance, int age) const {
	JavaRandom& random = level.random();
	int			odds   = burnOdds(level.getBlockState(pos));
	if (random.nextInt(chance) >= odds) return;
	int state = level.getBlockState(pos);
	if (random.nextInt(age + 10) < 5 && !level.isRainingAt(pos)) {
		int newAge = std::min(age + random.nextInt(5) / 4, 15);
		level.setBlock(pos, getStateWithAge(level, pos, newAge), Level::UPDATE_ALL);
	} else {
		level.removeBlock(pos, false);
	}
	if (_context->blocks.blockOf(state) == _tnt) Explosions::primeTnt(level, pos, nullptr);
}

void FireBlock::tick(Level& level, const BlockPos& pos, int state) const {
	const BlockContext& c	   = *_context;
	JavaRandom&			random = level.random();
	level.scheduleTick(pos, c.blocks.blockOf(state), 30 + random.nextInt(10)); // getFireTickDelay
	// doFireTick is on; allowFireTicksAwayFromPlayer is off: only near a player
	if (!level.anyPlayerCloseEnoughForSpawning(pos)) return;
	if (!canSurvive(level, pos, state)) level.removeBlock(pos, false);
	int	 below		= level.getBlockState(pos.below());
	const std::string& infiniburn = level.dimensionType().infiniburn;
	bool forever	= !infiniburn.empty() && level.gameData().isInTag("minecraft:block", infiniburn.substr(infiniburn[0] == '#' ? 1 : 0), c.blocks.blockOf(below));
	int	 age		= c.blocks.getInt(state, _age);
	if (!forever && level.isRaining() && isNearRain(level, pos) && random.nextFloat() < 0.2F + age * 0.03F) {
		level.removeBlock(pos, false);
		return;
	}
	int newAge = std::min(15, age + random.nextInt(3) / 2);
	if (age != newAge) {
		state = c.blocks.withInt(state, _age, newAge);
		level.setBlock(pos, state, Level::UPDATE_INVISIBLE | Level::UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
	}
	if (!forever) {
		if (!isValidFireLocation(level, pos)) {
			if (!c.isFaceSturdy(level.getBlockState(pos.below()), Direction::Up) || age > 3) level.removeBlock(pos, false);
			return;
		}
		if (age == 15 && random.nextInt(4) == 0 && !canBurn(level.getBlockState(pos.below()))) {
			level.removeBlock(pos, false);
			return;
		}
	}
	bool burnout = level.gameData().isInTag("minecraft:worldgen/biome", "minecraft:increased_fire_burnout", level.getBiomeId(pos));
	int	 bonus	 = burnout ? -50 : 0;
	checkBurnOut(level, pos.east(), 300 + bonus, age);
	checkBurnOut(level, pos.west(), 300 + bonus, age);
	checkBurnOut(level, pos.below(), 250 + bonus, age);
	checkBurnOut(level, pos.above(), 250 + bonus, age);
	checkBurnOut(level, pos.north(), 300 + bonus, age);
	checkBurnOut(level, pos.south(), 300 + bonus, age);
	for (int dx = -1; dx <= 1; dx++) {
		for (int dz = -1; dz <= 1; dz++) {
			for (int dy = -1; dy <= 4; dy++) {
				if (dx == 0 && dy == 0 && dz == 0) continue;
				int chance = 100;
				if (dy > 1) chance += (dy - 1) * 100;
				BlockPos at	  = pos.offset(dx, dy, dz);
				int		 odds = igniteOddsAround(level, at);
				if (odds <= 0) continue;
				int spread = (odds + 40 + level.difficulty() * 7) / (age + 30);
				if (burnout) spread /= 2;
				if (spread > 0 && random.nextInt(chance) <= spread && (!level.isRaining() || !isNearRain(level, at))) {
					int spreadAge = std::min(15, age + random.nextInt(5) / 4);
					level.setBlock(at, getStateWithAge(level, at, spreadAge), Level::UPDATE_ALL);
				}
			}
		}
	}
}

void FireBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const {
	BaseFireBlock::onPlace(level, pos, state, oldState, movedByPiston);
	level.scheduleTick(pos, _context->blocks.blockOf(state), 30 + level.random().nextInt(10));
}

// ===================== SoulFireBlock =====================

bool SoulFireBlock::canSurviveOnBlock(Level& level, int belowState) {
	return level.gameData().isInTag("minecraft:block", "minecraft:soul_fire_base_blocks", level.blocks().blockOf(belowState));
}

int SoulFireBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	return canSurvive(level, pos, state) ? _context->blocks.defaultState(_context->blocks.blockOf(state)) : _air;
}

bool SoulFireBlock::canSurvive(Level& level, const BlockPos& pos, int) const { return canSurviveOnBlock(level, level.getBlockState(pos.below())); }
