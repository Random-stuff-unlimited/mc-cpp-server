#ifndef END_DRAGON_FIGHT_HPP
#define END_DRAGON_FIGHT_HPP

#include "world/BlockPos.hpp"
#include "world/entity/Actor.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

class EndCrystal;
class Level;
namespace Combat {
	struct DamageSource;
}

// EndDragonFight: the End's boss fight. When a player first comes near the center it builds the arena (ten obsidian
// spikes, each with an end crystal on top, the two smallest in cages, and the unlit exit portal) and the dragon comes.
// Once the dragon is dead the exit portal opens (with the dragon egg on top, the first time). Saved in
// <world>/end_fight.dat. Respawning the dragon with four crystals and the end gateways aren't ported
class EndDragonFight {
  public:
	struct Spike {
		int centerX, centerZ, radius, height;
		bool guarded;
	};

	EndDragonFight(Level& level, std::filesystem::path file);

	bool			 dragonKilled() const { return _dragonKilled; }
	bool			 previouslyKilled() const { return _previouslyKilled; }
	bool			 arenaBuilt() const { return _arenaBuilt; }
	const BlockPos&	 portalLocation() const { return _portal; }
	// SpikeFeature.getSpikesForLevel: where the spikes stand (around 0, 0, 42 blocks away)
	static std::vector<Spike> spikes();

	void tick();
	// The dragon finished dying: the portal opens, the egg the first time
	void setDragonKilled();
	// A crystal broke: the dragon it was healing is hurt
	void onCrystalDestroyed(EndCrystal& crystal, const Combat::DamageSource& source);
	void save();

  private:
	Level&				  _level;
	std::filesystem::path _file;
	bool				  _arenaBuilt = false, _dragonKilled = false, _previouslyKilled = false;
	EntityRef			  _dragon;
	BlockPos			  _portal{0, 0, 0};
	int					  _ticks = 0, _missingDragonTicks = 0;
	bool				  _dirty = false;

	void buildArena();
	void spawnExitPortal(bool active);
	void spawnDragon();
	bool playersInArena();
	void load();
};

#endif
