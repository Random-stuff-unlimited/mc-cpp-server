#include "world/entity/LightningBolt.hpp"

#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/PoiManager.hpp"
#include "world/blocks/Fire.hpp"
#include "world/entity/mobs/Creeper.hpp"
#include "world/entity/mobs/FarmAnimals.hpp"
#include "world/entity/mobs/Turtle.hpp"
#include "world/entity/mobs/Horses.hpp"
#include "world/entity/mobs/Villagers.hpp"

#include <limits>

namespace {
	const Combat::DamageSource LIGHTNING{"minecraft:lightning_bolt", nullptr, nullptr, std::nullopt};

	// Entity.thunderHit's fire: one more tick, or 8 seconds when it wasn't burning
	void ignite(int& ticks) { ticks = ticks + 1 == 0 ? 160 : std::max(ticks + 1, 160); }
} // namespace

// Entity.thunderHit: set on fire (8 seconds), 5 damage
void Entity::thunderHit(LightningBolt&) {
	int fire = remainingFireTicks();
	ignite(fire);
	setRemainingFireTicks(fire);
	hurtServer(LIGHTNING, 5.0F);
}

LightningBolt::LightningBolt(Level& level) : Entity(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:lightning_bolt"), 0.0F, 0.0F) {
	_noGravity = true;
	_flashes   = level.random().nextInt(3) + 1;
}

BlockPos LightningBolt::strikePosition() const { return {Mth::floor(_position.x), Mth::floor(_position.y - 1.0E-6), Mth::floor(_position.z)}; }

// LightningBolt.spawnFire: fire where it hit, and `extra` more around it, in normal and hard
void LightningBolt::spawnFire(int extra) {
	if (_visualOnly || _level.difficulty() < 2) return;
	BlockPos pos   = blockPosition();
	auto	 light = [&](const BlockPos& at) {
		int fire = BaseFireBlock::getState(_level, at);
		if (_level.blocks().isAir(_level.getBlockState(at)) && _level.behavior(fire).canSurvive(_level, at, fire)) _level.setBlock(at, fire, Level::UPDATE_ALL);
	};
	light(pos);
	JavaRandom& random = _level.random();
	for (int i = 0; i < extra; i++) light(pos.offset(random.nextInt(3) - 1, random.nextInt(3) - 1, random.nextInt(3) - 1));
}

// LightningRodBlock.onLightningStrike: the rod it hit gives a signal for 8 ticks
void LightningBolt::powerLightningRod() {
	BlockPos		strike = strikePosition();
	int				state  = _level.getBlockState(strike);
	const GameData& data   = _level.gameData();
	if (data.getStaticName("minecraft:block", _level.blocks().blockOf(state)).find("lightning_rod") == std::string::npos) return;
	int powered = data.withProperty(state, "powered", "true");
	if (powered < 0 || powered == state) return;
	_level.setBlock(strike, powered, Level::UPDATE_ALL);
	_level.scheduleTick(strike, _level.blocks().blockOf(state), 8);
}

// LightningBolt.clearCopperOnLightningStrike: the copper hit loses all its oxidation, and a few random walks from it
// take a step of oxidation off the copper around
void LightningBolt::clearCopper() {
	const GameData& data = _level.gameData();
	auto			nameAt = [&](const BlockPos& pos) { return data.getStaticName("minecraft:block", _level.blocks().blockOf(_level.getBlockState(pos))); };
	// WeatheringCopper.getPrevious: one step less (copper names: exposed_, weathered_, oxidized_ prefixes, not waxed)
	auto previous = [&](const std::string& name) -> std::string {
		if (name.find("waxed") != std::string::npos) return "";
		for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{{"oxidized_", "weathered_"}, {"weathered_", "exposed_"}, {"exposed_", ""}}) {
			size_t at = name.find(from);
			if (at == std::string::npos) continue;
			std::string result = name.substr(0, at) + to + name.substr(at + from.size());
			if (result == "minecraft:copper") result = "minecraft:copper_block";
			return result;
		}
		return "";
	};
	auto setTo = [&](const BlockPos& pos, const std::string& name) {
		int state = _level.getBlockState(pos), next = data.getDefaultBlockState(name);
		if (next < 0) return false;
		// Same properties where they apply (a lightning rod's facing, a stair's shape...)
		std::string full = data.getBlockStateName(state);
		size_t		bracket = full.find('[');
		if (bracket != std::string::npos) {
			int same = data.getBlockStateFromName(name + full.substr(bracket));
			if (same >= 0) next = same;
		}
		_level.setBlock(pos, next, Level::UPDATE_ALL);
		return true;
	};
	BlockPos strike = strikePosition();
	std::string name = nameAt(strike);
	if (name.find("copper") == std::string::npos && name.find("lightning_rod") == std::string::npos) return;
	// getFirst: all the way back
	for (std::string back = previous(name); !back.empty(); back = previous(back)) name = back, setTo(strike, back);
	JavaRandom& random = _level.random();
	int			walks  = random.nextInt(3) + 3;
	for (int walk = 0; walk < walks; walk++) {
		BlockPos at	   = strike;
		int		 steps = random.nextInt(8) + 1;
		for (int step = 0; step < steps; step++) {
			// randomStepCleaningCopper: to a random copper neighbor (within one block), a step of oxidation off
			std::vector<BlockPos> copper;
			for (int dx = -1; dx <= 1; dx++) {
				for (int dy = -1; dy <= 1; dy++) {
					for (int dz = -1; dz <= 1; dz++) {
						BlockPos next = at.offset(dx, dy, dz);
						if (!(dx == 0 && dy == 0 && dz == 0) && !previous(nameAt(next)).empty()) copper.push_back(next);
					}
				}
			}
			if (copper.empty()) break;
			at = copper[random.nextInt(static_cast<int>(copper.size()))];
			setTo(at, previous(nameAt(at)));
		}
	}
}

void LightningBolt::tick() {
	if (_life == 2 && !_visualOnly) {
		spawnFire(4);
		powerLightningRod();
		clearCopper();
	}
	_life--;
	if (_life < 0) {
		if (_flashes == 0) {
			discard();
			return;
		}
		if (_life < -_level.random().nextInt(10)) {
			_flashes--;
			_life = 1;
			spawnFire(0);
		}
	}
	if (_life < 0 || _visualOnly) return;
	// What is within 3 blocks (6 above) is struck, once
	AABB area{_position.x - 3.0, _position.y - 3.0, _position.z - 3.0, _position.x + 3.0, _position.y + 9.0, _position.z + 3.0};
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || player->isSpectator() || player->combat().dead || _hit.count(player->id())) continue;
		if (!player->boundingBox().intersects(area)) continue;
		_hit.insert(player->id());
		int fire = player->survival().remainingFireTicks;
		ignite(fire);
		player->survival().remainingFireTicks = fire;
		Combat::damage(_level.server(), *player, 5.0F, LIGHTNING);
	}
	std::vector<Entity*> struck;
	_level.entities().forEachIn(area, [&](Entity& entity) {
		if (&entity != this && !entity.isRemoved() && entity.isAlive() && !_hit.count(entity.id())) struck.push_back(&entity);
	});
	for (Entity* entity : struck) {
		_hit.insert(entity->id());
		entity->thunderHit(*this);
	}
}

BlockPos LightningBolt::findTarget(Level& level, const BlockPos& around) {
	int		 top = level.getHeight(Level::Heightmap::MotionBlocking, around.x, around.z);
	BlockPos pos{around.x, top, around.z};
	// findLightningRod: the closest rod within 128 blocks that is the highest block of its column
	const PoiManager::Record* closest = nullptr;
	double					  best	  = std::numeric_limits<double>::max();
	for (const PoiManager::Record* record : level.poi().getInRange(PoiManager::Type::LightningRod, pos, 128, PoiManager::Occupancy::Any)) {
		if (record->pos.y != level.getHeight(Level::Heightmap::WorldSurface, record->pos.x, record->pos.z) - 1) continue;
		double dx = record->pos.x - pos.x, dy = record->pos.y - pos.y, dz = record->pos.z - pos.z;
		if (dx * dx + dy * dy + dz * dz < best) {
			best	= dx * dx + dy * dy + dz * dz;
			closest = record;
		}
	}
	if (closest) return closest->pos.relative(Direction::Up);
	// A living thing under the sky in the column (3 blocks around)
	AABB					   column{pos.x - 3.0, pos.y - 3.0, pos.z - 3.0, pos.x + 4.0, static_cast<double>(level.maxY()) + 3.0, pos.z + 4.0};
	std::vector<BlockPos>	   targets;
	for (Actor* actor : level.actorsIn(column)) {
		if (actor->isAlive() && (actor->asLiving() || actor->asPlayer())) {
			BlockPos at{Mth::floor(actor->position().x), Mth::floor(actor->position().y), Mth::floor(actor->position().z)};
			if (level.canSeeSky(at)) targets.push_back(at);
		}
	}
	if (!targets.empty()) return targets[level.random().nextInt(static_cast<int>(targets.size()))];
	if (pos.y == level.minY() - 1) pos = pos.relative(Direction::Up, 2);
	return pos;
}

void LightningBolt::tickThunder(Level& level, int chunkX, int chunkZ) {
	if (!level.isRaining() || !level.isThundering() || level.random().nextInt(100000) != 0) return;
	BlockPos pos = findTarget(level, level.getBlockRandomPos(chunkX * 16, 0, chunkZ * 16, 15));
	if (!level.isRainingAt(pos)) return;
	// A skeleton trap horse instead of a real bolt, now and then (more in harder places), not on a lightning rod
	DifficultyInstance difficulty = level.getCurrentDifficultyAt(pos);
	bool trap = level.mobSpawning() && level.random().nextDouble() < difficulty.getEffectiveDifficulty() * 0.01 &&
				level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(level.getBlockState(pos.below()))).find("lightning_rod") == std::string::npos;
	if (trap) {
		std::unique_ptr<Mob> mob = level.mobs().create(level, level.gameData().getStaticId("minecraft:entity_type", "minecraft:skeleton_horse"));
		if (auto* horse = dynamic_cast<UndeadHorse*>(mob.get())) {
			horse->setTrap(true);
			horse->setAge(0);
			horse->snapTo({pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}, 0.0F, 0.0F);
			level.addFreshEntity(std::move(mob));
		}
	}
	auto bolt = std::make_unique<LightningBolt>(level);
	bolt->setVisualOnly(trap);
	bolt->snapTo({pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}, 0.0F, 0.0F);
	level.entities().add(std::move(bolt));
}

// ----- What lightning does to some mobs -----

void Creeper::thunderHit(LightningBolt& bolt) {
	Entity::thunderHit(bolt);
	setPowered(true);
}

void Pig::thunderHit(LightningBolt& bolt) {
	if (_level.difficulty() == 0) return Entity::thunderHit(bolt);
	int type = _level.gameData().getStaticId("minecraft:entity_type", "minecraft:zombified_piglin");
	convertTo(type, false, true, [this](Mob& piglin) {
		piglin.setItemSlot(EquipmentSlot::MainHand, ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:golden_sword"), 1));
		piglin.setPersistenceRequired();
	});
}

void Villager::thunderHit(LightningBolt& bolt) {
	if (_level.difficulty() == 0) return Entity::thunderHit(bolt);
	convertTo(_level.gameData().getStaticId("minecraft:entity_type", "minecraft:witch"), false, false, [](Mob& witch) { witch.setPersistenceRequired(); });
}

void Mooshroom::thunderHit(LightningBolt& bolt) {
	// lastLightningBoltUUID: a bolt strikes for several ticks, the color changes once
	if (_lastBolt == bolt.uuid()) return;
	_lastBolt = bolt.uuid();
	setVariant(variant() == Red ? Brown : Red);
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.mooshroom.convert", Level::SoundSource::Neutral, 2.0F, 1.0F);
}

void Turtle::thunderHit(LightningBolt&) { hurtServer({"minecraft:lightning_bolt", nullptr, nullptr, std::nullopt}, std::numeric_limits<float>::max()); }
