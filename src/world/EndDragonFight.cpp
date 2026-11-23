#include "world/EndDragonFight.hpp"

#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/EndCrystal.hpp"
#include "world/entity/MobRegistry.hpp"
#include "world/entity/mobs/Bosses.hpp"

#include <cmath>
#include <fstream>

namespace {
	constexpr int ARENA_RADIUS = 192, FILE_VERSION = 1;
}

EndDragonFight::EndDragonFight(Level& level, std::filesystem::path file) : _level(level), _file(std::move(file)) { load(); }

std::vector<EndDragonFight::Spike> EndDragonFight::spikes() {
	// SpikeFeature: ten spikes on a circle of 42, their sizes shuffled (by the seed in vanilla: a fixed order here)
	static const int ORDER[10] = {3, 7, 0, 9, 5, 1, 8, 4, 2, 6};
	std::vector<Spike> result;
	for (int i = 0; i < 10; i++) {
		double angle = 2.0 * (-M_PI + (M_PI / 10.0) * i);
		int	   size	 = ORDER[i];
		result.push_back({static_cast<int>(std::floor(42.0 * std::cos(angle))), static_cast<int>(std::floor(42.0 * std::sin(angle))), 2 + size / 3,
						  76 + size * 3, size == 1 || size == 2});
	}
	return result;
}

bool EndDragonFight::playersInArena() {
	for (const auto& player : _level.players()) {
		if (player->isDisconnected() || !player->isAlive() || player->isSpectator()) continue;
		if (std::abs(player->position().x) < ARENA_RADIUS && std::abs(player->position().z) < ARENA_RADIUS) return true;
	}
	return false;
}

void EndDragonFight::buildArena() {
	// The chunks of the arena, loaded now (once)
	for (int cx = -4; cx <= 4; cx++) {
		for (int cz = -4; cz <= 4; cz++) _level.loadChunkNow(cx, cz);
	}
	const GameData& data	 = _level.gameData();
	int				obsidian = data.getDefaultBlockState("minecraft:obsidian"), bedrock = data.getDefaultBlockState("minecraft:bedrock");
	int				air		 = data.getDefaultBlockState("minecraft:air"), bars = data.getDefaultBlockState("minecraft:iron_bars");
	// The ground: the top of the island at the center
	int ground = _level.minY();
	for (int y = std::min(_level.maxY() - 1, 255); y > _level.minY(); y--) {
		if (!_level.blocks().isAir(_level.getBlockState({0, y, 0}))) {
			ground = y;
			break;
		}
	}
	for (const Spike& spike : spikes()) {
		// SpikeFeature.place: an obsidian cylinder up to its height, cleared above; bedrock and a crystal on top
		for (int x = spike.centerX - spike.radius; x <= spike.centerX + spike.radius; x++) {
			for (int z = spike.centerZ - spike.radius; z <= spike.centerZ + spike.radius; z++) {
				int dx = x - spike.centerX, dz = z - spike.centerZ;
				if (dx * dx + dz * dz > spike.radius * spike.radius + 1) continue;
				for (int y = ground; y < spike.height; y++) _level.setBlock({x, y, z}, obsidian, Level::UPDATE_CLIENTS);
				for (int y = spike.height; y < spike.height + 10; y++) _level.setBlock({x, y, z}, air, Level::UPDATE_CLIENTS);
			}
		}
		if (spike.guarded) {
			// The cage of iron bars around its top
			for (int dx = -2; dx <= 2; dx++) {
				for (int dz = -2; dz <= 2; dz++) {
					for (int dy = 0; dy <= 3; dy++) {
						bool wall = std::abs(dx) == 2 || std::abs(dz) == 2 || dy == 3;
						if (wall) _level.setBlock({spike.centerX + dx, spike.height + dy, spike.centerZ + dz}, bars, Level::UPDATE_CLIENTS);
					}
				}
			}
		}
		_level.setBlock({spike.centerX, spike.height, spike.centerZ}, bedrock, Level::UPDATE_CLIENTS);
		_level.entities().add(std::make_unique<EndCrystal>(_level, Vec3{spike.centerX + 0.5, spike.height + 1.0, spike.centerZ + 0.5}));
	}
	_portal = {0, ground + 1, 0};
	spawnExitPortal(false);
	_arenaBuilt = true;
	_dirty		= true;
}

void EndDragonFight::spawnExitPortal(bool active) {
	// EndPodiumFeature: a bedrock bowl (end stone under it), the end portal inside when active, a pillar with torches
	const GameData& data	  = _level.gameData();
	int				bedrock	  = data.getDefaultBlockState("minecraft:bedrock"), endStone = data.getDefaultBlockState("minecraft:end_stone");
	int				air		  = data.getDefaultBlockState("minecraft:air"), portal = data.getDefaultBlockState("minecraft:end_portal");
	const BlockPos& o		  = _portal;
	for (int dx = -4; dx <= 4; dx++) {
		for (int dz = -4; dz <= 4; dz++) {
			double d = std::sqrt(static_cast<double>(dx * dx + dz * dz));
			if (d > 3.5) continue;
			for (int dy = -1; dy <= 32; dy++) {
				BlockPos pos{o.x + dx, o.y + dy, o.z + dz};
				if (dy < 0) {
					_level.setBlock(pos, d <= 2.5 ? bedrock : endStone, Level::UPDATE_CLIENTS);
				} else if (dy > 0) {
					_level.setBlock(pos, air, Level::UPDATE_CLIENTS);
				} else if (d > 2.5) {
					_level.setBlock(pos, bedrock, Level::UPDATE_CLIENTS);
				} else {
					_level.setBlock(pos, active ? portal : air, Level::UPDATE_CLIENTS);
				}
			}
		}
	}
	for (int dy = 0; dy < 4; dy++) _level.setBlock(o.offset(0, dy, 0), bedrock, Level::UPDATE_CLIENTS);
	for (const char* facing : {"north", "south", "west", "east"}) {
		int torch = data.getBlockState("minecraft:wall_torch", {{"facing", facing}});
		int dx = std::string(facing) == "west" ? -1 : std::string(facing) == "east" ? 1 : 0;
		int dz = std::string(facing) == "north" ? -1 : std::string(facing) == "south" ? 1 : 0;
		if (torch >= 0) _level.setBlock(o.offset(dx, 2, dz), torch, Level::UPDATE_CLIENTS);
	}
}

void EndDragonFight::spawnDragon() {
	Mob* dragon = _level.mobs().spawn(_level, _level.gameData().getStaticId("minecraft:entity_type", "minecraft:ender_dragon"), {0, 128, 0},
									  MobRegistry::SpawnReason::Event);
	if (dragon) {
		_dragon = EntityRef(*dragon);
		_dirty	= true;
	}
}

void EndDragonFight::tick() {
	if (++_ticks % 20 != 0 || !playersInArena()) return;
	if (!_arenaBuilt) buildArena();
	if (_dragonKilled) return;
	// The dragon: the one it knows, else any in the level, else a new one after a while without
	Actor* dragon = _dragon.isSet() ? _level.actorByRef(_dragon) : nullptr;
	if (!dragon) {
		_level.entities().forEach([&](Entity& entity) {
			if (!dragon && dynamic_cast<EnderDragon*>(&entity) && !entity.isRemoved()) dragon = &entity;
		});
		if (dragon) _dragon = EntityRef(*dragon);
	}
	if (dragon) {
		_missingDragonTicks = 0;
		return;
	}
	if ((_missingDragonTicks += 20) >= (_dragon.isSet() ? 1200 : 20)) {
		_missingDragonTicks = 0;
		spawnDragon();
	}
}

void EndDragonFight::setDragonKilled() {
	_dragonKilled = true;
	if (_arenaBuilt) {
		spawnExitPortal(true);
		if (!_previouslyKilled) _level.setBlock(_portal.offset(0, 4, 0), _level.gameData().getDefaultBlockState("minecraft:dragon_egg"), Level::UPDATE_ALL);
	}
	_previouslyKilled = true;
	_dirty			  = true;
	save();
}

void EndDragonFight::onCrystalDestroyed(EndCrystal& crystal, const Combat::DamageSource& source) {
	// EnderDragon.onCrystalDestroyed: the crystal it was drawing from blows up in its face (10 damage)
	Actor* dragon = _dragon.isSet() ? _level.actorByRef(_dragon) : nullptr;
	auto*  ender  = dynamic_cast<EnderDragon*>(dragon);
	if (!ender || !ender->isAlive() || ender->healingCrystalId() != crystal.id()) return;
	Combat::DamageSource blast{"minecraft:explosion", source.causing, &crystal, std::nullopt};
	ender->hurtServer(blast, 10.0F);
}

void EndDragonFight::save() {
	if (!_dirty) return;
	_dirty = false;
	std::ofstream out(_file, std::ios::binary);
	uint8_t		  flags = (_arenaBuilt ? 1 : 0) | (_dragonKilled ? 2 : 0) | (_previouslyKilled ? 4 : 0);
	int32_t		  pos[3] = {_portal.x, _portal.y, _portal.z};
	out.put(static_cast<char>(FILE_VERSION));
	out.put(static_cast<char>(flags));
	out.write(reinterpret_cast<const char*>(pos), sizeof(pos));
}

void EndDragonFight::load() {
	std::ifstream in(_file, std::ios::binary);
	if (!in) return;
	int version = in.get(), flags = in.get();
	int32_t pos[3] = {0, 0, 0};
	in.read(reinterpret_cast<char*>(pos), sizeof(pos));
	if (version != FILE_VERSION || !in) return;
	_arenaBuilt		  = flags & 1;
	_dragonKilled	  = flags & 2;
	_previouslyKilled = flags & 4;
	_portal			  = {pos[0], pos[1], pos[2]};
}
