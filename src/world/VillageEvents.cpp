#include <algorithm>
#include <fstream>
#include "logger.hpp"
#include "lib/json.hpp"
#include "world/VillageEvents.hpp"

#include "player.hpp"
#include "world/Level.hpp"
#include "world/PoiManager.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/Bosses.hpp"
#include "world/entity/mobs/HelperMobs.hpp"
#include "world/entity/mobs/Illagers.hpp"
#include "world/entity/mobs/IronGolem.hpp"
#include "world/entity/mobs/Villagers.hpp"

#include <cmath>

namespace {
	constexpr double RAID_RANGE = 96.0;

	// The top of the ground at x, z near y: a block with two free ones above, searched 30 blocks up and down
	std::optional<BlockPos> surfaceAt(Level& level, int x, int z, int aroundY) {
		for (int y = aroundY + 30; y > aroundY - 30; y--) {
			if (y <= level.minY() || y + 2 >= level.maxY()) continue;
			int ground = level.getBlockState({x, y, z});
			if (level.gameData().getCollisionShape(ground).empty()) continue;
			if (!level.gameData().getCollisionShape(level.getBlockState({x, y + 1, z})).empty()) continue;
			if (!level.gameData().getCollisionShape(level.getBlockState({x, y + 2, z})).empty()) continue;
			if (!level.getFluidState({x, y + 1, z}).isEmpty()) continue;
			return BlockPos{x, y + 1, z};
		}
		return std::nullopt;
	}

	BlockPos blockPosOf(Actor& actor) { return {Mth::floor(actor.position().x), Mth::floor(actor.position().y), Mth::floor(actor.position().z)}; }

	int typeId(Level& level, const char* name) { return level.gameData().getStaticId("minecraft:entity_type", name); }

	// Raid.RaiderType: how many of each come in each wave (index 1 to 7)
	struct RaiderWave {
		const char* type;
		int			counts[8];
	};
	const RaiderWave RAIDER_WAVES[] = {{"minecraft:vindicator", {0, 0, 2, 0, 1, 4, 2, 5}},
									   {"minecraft:evoker", {0, 0, 0, 0, 1, 1, 0, 2}},
									   {"minecraft:pillager", {0, 4, 3, 3, 4, 4, 4, 2}},
									   {"minecraft:witch", {0, 0, 0, 0, 3, 0, 0, 1}},
									   {"minecraft:ravager", {0, 0, 0, 1, 0, 1, 0, 2}}};
} // namespace

// ===================== Raid =====================

Raid::Raid(Level& level, int id, const BlockPos& center, int omenLevel)
	: _level(level), _id(id), _center(center), _omenLevel(std::max(1, omenLevel)),
	  _bar(std::make_unique<BossBar>(level, "event.minecraft.raid", BossBar::Red, 0, 2)) {
	// Raid.getNumGroups: by difficulty, one more for a strong omen
	int difficulty = level.difficulty();
	_numGroups	   = difficulty == 1 ? 3 : difficulty == 2 ? 5 : difficulty == 3 ? 7 : 0;
	if (_omenLevel > 1 && _numGroups > 0) _numGroups++;
	_bar->setProgress(0.0F);
	if (_numGroups == 0) _status = Status::Stopped;
}

Raid::~Raid() = default;

int Raid::raidersAlive() {
	int alive = 0;
	for (const EntityRef& ref : _raiders) {
		Actor* actor = _level.actorByRef(ref);
		if (actor && actor->isAlive()) alive++;
	}
	return alive;
}

float Raid::healthOfRaiders() {
	float health = 0.0F;
	for (const EntityRef& ref : _raiders) {
		Actor* actor = _level.actorByRef(ref);
		if (LivingEntity* living = actor ? actor->asLiving() : nullptr; living && living->isAlive()) health += living->health();
	}
	return health;
}

std::optional<BlockPos> Raid::findSpawnPos() {
	JavaRandom& random = _level.random();
	for (int attempt = 0; attempt < 20; attempt++) {
		float angle = random.nextFloat() * (float)M_PI * 2.0F;
		int	  x		= _center.x + Mth::floor(Mth::cos(angle) * 32.0F) + random.nextInt(5);
		int	  z		= _center.z + Mth::floor(Mth::sin(angle) * 32.0F) + random.nextInt(5);
		if (std::optional<BlockPos> pos = surfaceAt(_level, x, z, _center.y)) return pos;
	}
	return std::nullopt;
}

void Raid::spawnGroup() {
	// Raid.spawnGroup: the wave's raiders at one spot outside the village, the first of them its captain
	std::optional<BlockPos> spot = findSpawnPos();
	if (!spot) spot = _center;
	int	 wave	   = std::min(_groupsSpawned + 1, 7);
	bool leader	   = false;
	_waveHealth	   = 0.0F;
	JavaRandom& random = _level.random();
	for (const RaiderWave& kind : RAIDER_WAVES) {
		for (int i = 0; i < kind.counts[wave]; i++) {
			BlockPos at	 = spot->offset(random.nextInt(3) - 1, 0, random.nextInt(3) - 1);
			Mob*	 mob = _level.mobs().spawn(_level, typeId(_level, kind.type), at, MobRegistry::SpawnReason::Event);
			if (!mob) continue;
			mob->setPersistenceRequired();
			if (auto* raider = dynamic_cast<Raider*>(mob)) {
				raider->setRaidId(_id);
				if (!leader) {
					raider->setPatrolLeader(true);
					leader = true;
				}
			}
			_raiders.push_back(EntityRef(*mob));
			_waveHealth += mob->maxHealth();
		}
	}
	_groupsSpawned++;
	// The raid horn, for the players around
	for (const auto& player : _level.players()) {
		Vec3 d = player->position() - Vec3{_center.x + 0.5, static_cast<double>(_center.y), _center.z + 0.5};
		if (d.lengthSqr() < 64.0 * 64.0) {
			_level.playSoundAt(nullptr, player->position().x, player->position().y, player->position().z, "minecraft:event.raid.horn",
							   Level::SoundSource::Neutral, 64.0F, 1.0F);
		}
	}
}

void Raid::tick() {
	Vec3 center{_center.x + 0.5, static_cast<double>(_center.y), _center.z + 0.5};
	// After a load: its raiders are the ones that say they belong to it
	if (!_reattached) {
		_reattached = true;
		_level.entities().forEach([&](Entity& entity) {
			auto* raider = dynamic_cast<Raider*>(&entity);
			if (raider && raider->raidId() == _id && raider->isAlive() &&
				std::none_of(_raiders.begin(), _raiders.end(), [&](const EntityRef& ref) { return _level.actorByRef(ref) == raider; })) {
				_raiders.push_back(EntityRef(*raider));
			}
		});
	}
	if (_status != Status::Ongoing) {
		if (_celebrationTicks > 0) _celebrationTicks--;
		return;
	}
	if (++_ticksActive >= MAX_TICKS) {
		stop();
		return;
	}
	// The village is gone: lost (stopped if no wave came yet)
	if (_ticksActive % 20 == 0 && !_level.isVillage(_center)) {
		if (_groupsSpawned > 0) {
			lose();
		} else {
			stop();
		}
		return;
	}
	if (raidersAlive() == 0) {
		if (_groupsSpawned >= _numGroups) {
			win();
			return;
		}
		// Raid.shouldSpawnGroup: after the countdown (its bar fills)
		if (--_cooldownTicks <= 0) {
			spawnGroup();
			_cooldownTicks = PRE_RAID_TICKS;
		}
		_bar->setProgress(1.0F - static_cast<float>(_cooldownTicks) / PRE_RAID_TICKS);
	} else {
		_bar->setProgress(_waveHealth > 0.0F ? healthOfRaiders() / _waveHealth : 0.0F);
	}
	_bar->update(center, RAID_RANGE);
}

void Raid::win() {
	// The players around are the Heroes of the Village (its level by the omen's)
	_status = Status::Victory;
	_bar->removeAll();
	Vec3 center{_center.x + 0.5, static_cast<double>(_center.y), _center.z + 0.5};
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || player->isSpectator() || (player->position() - center).lengthSqr() > RAID_RANGE * RAID_RANGE) continue;
		player->addEffect({MobEffects::HeroOfTheVillage, 48000, _omenLevel - 1});
	}
}

void Raid::lose() {
	// The raiders celebrate
	_status			  = Status::Loss;
	_celebrationTicks = CELEBRATION_TICKS;
	_bar->removeAll();
	for (const EntityRef& ref : _raiders) {
		if (auto* raider = dynamic_cast<Raider*>(_level.actorByRef(ref))) raider->setCelebrating(true);
	}
}

void Raid::stop() {
	_status = Status::Stopped;
	_bar->removeAll();
	for (const EntityRef& ref : _raiders) {
		if (auto* raider = dynamic_cast<Raider*>(_level.actorByRef(ref))) raider->setRaidId(-1);
	}
}

void Raid::onRaiderDied(Raider&) {} // Counted on its next tick

// ===================== VillageEvents =====================

Raid* VillageEvents::raid(int id) {
	auto it = _raids.find(id);
	return it == _raids.end() ? nullptr : it->second.get();
}

Raid* VillageEvents::raidAt(const BlockPos& pos) {
	for (const auto& [id, raid] : _raids) {
		if (!raid->isOngoing()) continue;
		int64_t dx = raid->center().x - pos.x, dy = raid->center().y - pos.y, dz = raid->center().z - pos.z;
		if (dx * dx + dy * dy + dz * dz < static_cast<int64_t>(RAID_RANGE * RAID_RANGE)) return raid.get();
	}
	return nullptr;
}

Raid* VillageEvents::startRaid(const BlockPos& pos, int omenLevel) {
	if (Raid* existing = raidAt(pos)) return existing;
	int	 id	  = _nextRaidId++;
	auto raid = std::make_unique<Raid>(_level, id, pos, omenLevel);
	Raid* added = raid.get();
	_raids[id]	= std::move(raid);
	return added;
}

void VillageEvents::onRaiderDied(Raider& raider) {
	if (Raid* r = raid(raider.raidId())) r->onRaiderDied(raider);
}

void VillageEvents::tickOmens() {
	// Bad Omen in a village becomes Raid Omen (30 seconds); when it ends, the raid starts where it began
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || !player->isAlive() || player->isSpectator()) continue;
		int id = player->getPlayerID();
		if (const MobEffects::Instance* omen = player->effects().get(MobEffects::BadOmen)) {
			BlockPos pos = blockPosOf(*player);
			if (_level.difficulty() != 0 && _level.isVillage(pos) && !raidAt(pos)) {
				int amplifier = omen->amplifier;
				player->removeEffect(MobEffects::BadOmen);
				player->addEffect({MobEffects::RaidOmen, 600, amplifier});
				_omenPositions[id] = pos;
				_level.playSoundAt(nullptr, player->position().x, player->position().y, player->position().z, "minecraft:event.mob_effect.raid_omen",
								   Level::SoundSource::Neutral, 1.0F, 1.0F);
			}
		}
		auto it = _omenPositions.find(id);
		if (it != _omenPositions.end() && !player->hasEffect(MobEffects::RaidOmen)) {
			startRaid(it->second, 1);
			_omenPositions.erase(it);
		}
	}
}

std::optional<BlockPos> VillageEvents::surfaceNear(const BlockPos& pos) { return surfaceAt(_level, pos.x, pos.z, pos.y); }

int VillageEvents::spawnPatrol(const BlockPos& pos, int count) {
	JavaRandom& random	= _level.random();
	int			spawned = 0;
	for (int i = 0; i < count; i++) {
		BlockPos				at		= i == 0 ? pos : pos.offset(random.nextInt(5) - 2, 0, random.nextInt(5) - 2);
		std::optional<BlockPos> surface = surfaceNear(at);
		if (!surface) continue;
		Mob*  mob	 = _level.mobs().spawn(_level, typeId(_level, "minecraft:pillager"), *surface, MobRegistry::SpawnReason::Patrol);
		auto* raider = dynamic_cast<Raider*>(mob);
		if (!raider) continue;
		if (spawned == 0) {
			raider->setPatrolLeader(true);
			raider->findPatrolTarget();
		} else {
			raider->setPatrolling(true);
		}
		spawned++;
	}
	return spawned;
}

void VillageEvents::tickPatrolSpawner() {
	// PatrolSpawner: every 10 to 11 minutes, from the fifth day, in daylight, 1 time in 5, 24 to 47 blocks from a player
	if (--_patrolTick > 0) return;
	JavaRandom& random = _level.random();
	_patrolTick += 12000 + random.nextInt(1200);
	if (_level.getDayTime() / 24000 < 5 || !_level.isBrightOutside() || random.nextInt(5) != 0) return;
	std::vector<Player*> players;
	for (const auto& player : _level.players()) {
		if (!player->isDisconnected() && player->isAlive() && !player->isSpectator()) players.push_back(player.get());
	}
	if (players.empty()) return;
	Player* player = players[random.nextInt(static_cast<int>(players.size()))];
	if (_level.isCloseToVillage(blockPosOf(*player), 2)) return;
	int		 dx	 = (24 + random.nextInt(24)) * (random.nextBoolean() ? -1 : 1);
	int		 dz	 = (24 + random.nextInt(24)) * (random.nextBoolean() ? -1 : 1);
	BlockPos pos = blockPosOf(*player).offset(dx, 0, dz);
	if (!_level.hasChunkAt(pos)) return;
	int count = static_cast<int>(std::ceil(_level.getCurrentDifficultyAt(pos).getEffectiveDifficulty())) + 1;
	spawnPatrol(pos, count);
}

bool VillageEvents::spawnWanderingTrader(Player& player) {
	// Near a village's bell if there is one around, else near the player
	BlockPos base = blockPosOf(player);
	for (const PoiManager::Record* bell : _level.poi().getInRange(PoiManager::Type::Meeting, base, 48, PoiManager::Occupancy::Any)) {
		base = bell->pos;
		break;
	}
	JavaRandom&				random = _level.random();
	std::optional<BlockPos> spot;
	for (int i = 0; i < 10 && !spot; i++) {
		int x = base.x + random.nextInt(97) - 48, z = base.z + random.nextInt(97) - 48;
		if (_level.hasChunkAt({x, base.y, z})) spot = surfaceAt(_level, x, z, base.y);
	}
	if (!spot) return false;
	auto* trader = dynamic_cast<WanderingTrader*>(_level.mobs().spawn(_level, typeId(_level, "minecraft:wandering_trader"), *spot, MobRegistry::SpawnReason::Event));
	if (!trader) return false;
	trader->setDespawnDelay(48000);
	trader->setWanderTarget(base);
	trader->setHomeTo(base, 16);
	// tryToSpawnLlamaFor: its two llamas, on leads it holds
	for (int i = 0; i < 2; i++) {
		BlockPos at = spot->offset(random.nextInt(5) - 2, 0, random.nextInt(5) - 2);
		if (std::optional<BlockPos> surface = surfaceNear(at)) {
			if (Mob* llama = _level.mobs().spawn(_level, typeId(_level, "minecraft:trader_llama"), *surface, MobRegistry::SpawnReason::Event)) {
				llama->setHomeTo(base, 16);
				llama->setLeashedTo(*trader);
			}
		}
	}
	return true;
}

void VillageEvents::tickWanderingTraderSpawner() {
	// WanderingTraderSpawner: a try every day (checked each minute), more likely each time it fails (25 to 75 %)
	if (--_traderTick > 0) return;
	_traderTick = 1200;
	_traderSpawnDelay -= 1200;
	if (_traderSpawnDelay > 0) return;
	_traderSpawnDelay	= 24000;
	int chance			= _traderSpawnChance;
	_traderSpawnChance	= std::max(25, std::min(75, _traderSpawnChance + 25));
	JavaRandom& random = _level.random();
	if (random.nextInt(100) > chance) return;
	std::vector<Player*> players;
	for (const auto& player : _level.players()) {
		if (!player->isDisconnected() && player->isAlive() && !player->isSpectator()) players.push_back(player.get());
	}
	if (players.empty() || random.nextInt(10) != 0) return;
	if (spawnWanderingTrader(*players[random.nextInt(static_cast<int>(players.size()))])) _traderSpawnChance = 25;
}

void VillageEvents::tick(bool spawners) {
	for (auto it = _raids.begin(); it != _raids.end();) {
		it->second->tick();
		if (it->second->isOver()) {
			it = _raids.erase(it);
		} else {
			++it;
		}
	}
	if (_level.getGameTime() % 20 == 0) tickOmens();
	if (spawners) {
		tickPatrolSpawner();
		tickWanderingTraderSpawner();
	}
}

// ===================== CarvedPumpkinBehavior =====================

void CarvedPumpkinBehavior::onPlace(Level& level, const BlockPos& pos, int, int oldState, bool) const {
	if (level.blocks().blockOf(oldState) == level.blocks().blockOf(level.getBlockState(pos))) return; // Only a new pumpkin
	const GameData& data = level.gameData();
	auto			name = [&](const BlockPos& p) { return data.getStaticName("minecraft:block", level.blocks().blockOf(level.getBlockState(p))); };
	auto			airAt = [&](const BlockPos& p) { return level.blocks().isAir(level.getBlockState(p)); };
	int				air	 = data.getDefaultBlockState("minecraft:air");
	auto			clear = [&](std::initializer_list<BlockPos> positions) {
		for (const BlockPos& p : positions) {
			level.levelEvent(nullptr, 2001, p, level.getBlockState(p)); // Break particles
			level.setBlock(p, air, Level::UPDATE_CLIENTS);
		}
	};
	auto spawn = [&](const char* type, const BlockPos& at) {
		return level.mobs().spawn(level, data.getStaticId("minecraft:entity_type", type), at, MobRegistry::SpawnReason::Triggered);
	};
	BlockPos b1 = pos.below(), b2 = pos.below().below();
	// Snow golem: two snow blocks
	if (name(b1) == "minecraft:snow_block" && name(b2) == "minecraft:snow_block") {
		clear({pos, b1, b2});
		spawn("minecraft:snow_golem", b2);
		return;
	}
	// Iron golem: a T of iron blocks (arms along x or z, nothing under them)
	if (name(b1) == "minecraft:iron_block" && name(b2) == "minecraft:iron_block") {
		for (int axis = 0; axis < 2; axis++) {
			int		 ax = axis == 0 ? 1 : 0, az = axis == 0 ? 0 : 1;
			BlockPos left = b1.offset(-ax, 0, -az), right = b1.offset(ax, 0, az);
			if (name(left) != "minecraft:iron_block" || name(right) != "minecraft:iron_block") continue;
			if (!airAt(left.below()) || !airAt(right.below())) continue;
			clear({pos, b1, b2, left, right});
			if (auto* golem = dynamic_cast<IronGolem*>(spawn("minecraft:iron_golem", b2))) golem->setPlayerCreated(true);
			return;
		}
	}
	// Copper golem: any block of copper (its oxidation and wax are the golem's)
	std::string copper = name(b1);
	bool		waxed  = copper.rfind("minecraft:waxed_", 0) == 0;
	std::string bare   = waxed ? "minecraft:" + copper.substr(16) : copper;
	CopperGolem::Weather weather;
	if (bare == "minecraft:copper_block") {
		weather = CopperGolem::Weather::Unaffected;
	} else if (bare == "minecraft:exposed_copper") {
		weather = CopperGolem::Weather::Exposed;
	} else if (bare == "minecraft:weathered_copper") {
		weather = CopperGolem::Weather::Weathered;
	} else if (bare == "minecraft:oxidized_copper") {
		weather = CopperGolem::Weather::Oxidized;
	} else {
		return;
	}
	clear({pos, b1});
	if (auto* golem = dynamic_cast<CopperGolem*>(spawn("minecraft:copper_golem", b1))) {
		golem->setWeather(weather);
		golem->setWaxed(waxed);
	}
}

// ----- Saving -----

void VillageEvents::load(const std::filesystem::path& file) {
	_file = file;
	std::ifstream in(file);
	if (!in) return;
	try {
		nlohmann::json data = nlohmann::json::parse(in);
		_patrolTick		   = data.value("patrol-tick", _patrolTick);
		_traderTick		   = data.value("trader-tick", _traderTick);
		_traderSpawnDelay  = data.value("trader-spawn-delay", _traderSpawnDelay);
		_traderSpawnChance = data.value("trader-spawn-chance", _traderSpawnChance);
		_nextRaidId		   = data.value("next-raid-id", _nextRaidId);
		const nlohmann::json raids = data.value("raids", nlohmann::json::array());
		for (const nlohmann::json& r : raids) {
			int		 id = r.value("id", 0);
			BlockPos center{r.value("x", 0), r.value("y", 0), r.value("z", 0)};
			auto	 raid		   = std::make_unique<Raid>(_level, id, center, r.value("omen-level", 1));
			raid->_numGroups	   = r.value("groups", raid->_numGroups);
			raid->_groupsSpawned   = r.value("groups-spawned", 0);
			raid->_status		   = static_cast<Raid::Status>(r.value("status", 0));
			raid->_ticksActive	   = r.value("ticks-active", 0);
			raid->_cooldownTicks   = r.value("cooldown", Raid::PRE_RAID_TICKS);
			raid->_celebrationTicks = r.value("celebration", 0);
			raid->_waveHealth	   = r.value("wave-health", 0.0F);
			_raids[id]			   = std::move(raid);
		}
	} catch (const std::exception& e) {
		g_logger->logGameInfo(ERROR, "Couldn't read " + file.string() + ": " + e.what(), "Raids");
	}
}

void VillageEvents::save() const {
	if (_file.empty()) return;
	nlohmann::json data = {{"patrol-tick", _patrolTick},	   {"trader-tick", _traderTick},	   {"trader-spawn-delay", _traderSpawnDelay},
						   {"trader-spawn-chance", _traderSpawnChance}, {"next-raid-id", _nextRaidId}, {"raids", nlohmann::json::array()}};
	for (const auto& [id, raid] : _raids) {
		data["raids"].push_back({{"id", id},
								 {"x", raid->_center.x},
								 {"y", raid->_center.y},
								 {"z", raid->_center.z},
								 {"omen-level", raid->_omenLevel},
								 {"groups", raid->_numGroups},
								 {"groups-spawned", raid->_groupsSpawned},
								 {"status", static_cast<int>(raid->_status)},
								 {"ticks-active", raid->_ticksActive},
								 {"cooldown", raid->_cooldownTicks},
								 {"celebration", raid->_celebrationTicks},
								 {"wave-health", raid->_waveHealth}});
	}
	std::ofstream out(_file);
	if (out) out << data.dump(1) << "\n";
}
