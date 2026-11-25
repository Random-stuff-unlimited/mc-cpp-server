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
// <world>/end_fight.dat. Each death opens an end gateway (20 at most) to the outer islands. Four crystals on the exit
// portal's sides bring the dragon back: the portal closes, the spikes are rebuilt one by one, the crystals blow up
// and a new dragon comes
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
	// EndDragonFight.tryRespawn (an end crystal was placed): four crystals around the exit portal start the respawn
	bool tryRespawn();
	bool isRespawning() const { return _respawnStage != RespawnStage::None; }
	int	 gatewaysLeft() const { return static_cast<int>(_gateways.size()); }
	// The 20 gateway positions, 96 blocks around the center at y 75, in their (seeded) order
	static std::vector<BlockPos> gatewayPositions(int64_t seed);

  private:
	Level&				  _level;
	std::filesystem::path _file;
	bool				  _arenaBuilt = false, _dragonKilled = false, _previouslyKilled = false;
	EntityRef			  _dragon;
	BlockPos			  _portal{0, 0, 0};
	int					  _ticks = 0, _missingDragonTicks = 0;
	bool				  _dirty = false;

	enum class RespawnStage { None, Start, PreparingPillars, SummoningPillars, SummoningDragon };
	RespawnStage			 _respawnStage = RespawnStage::None;
	int						 _respawnTime  = 0, _respawnSpike = 0;
	std::vector<EntityRef>	 _respawnCrystals;
	std::vector<int>		 _gateways; // Indexes in gatewayPositions still to open
	bool					 _gatewaysLoaded = false;

	void buildArena();
	void buildSpike(const Spike& spike, int ground, bool explode);
	int	 groundY();
	void spawnNewGateway();
	void tickRespawn();
	void spawnExitPortal(bool active);
	void spawnDragon();
	bool playersInArena();
	void load();
};

#endif
