#ifndef VILLAGE_EVENTS_HPP
#define VILLAGE_EVENTS_HPP

#include "world/BlockBehavior.hpp"
#include "world/BlockPos.hpp"
#include "world/entity/Actor.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class BossBar;
class Level;
class Player;
class Raider;

// Raid: waves of raiders (pillagers, vindicators, evokers, witches, ravagers) marching on a village, started by a
// player with Raid Omen. A wave comes 15 seconds after the last one died; when all are beaten the players around are
// the Heroes of the Village. Lost when the village is gone, stopped after 40 minutes. Its bar shows how many are left
class Raid {
  public:
	friend class VillageEvents; // Saves and loads its state
	enum class Status { Ongoing, Victory, Loss, Stopped };
	static constexpr int PRE_RAID_TICKS = 300, MAX_TICKS = 48000, CELEBRATION_TICKS = 600;

	Raid(Level& level, int id, const BlockPos& center, int omenLevel);
	~Raid();

	int				id() const { return _id; }
	const BlockPos& center() const { return _center; }
	Status			status() const { return _status; }
	bool			isOngoing() const { return _status == Status::Ongoing; }
	bool			isOver() const { return _status != Status::Ongoing && _celebrationTicks <= 0; }
	int				groupsSpawned() const { return _groupsSpawned; }
	int				numGroups() const { return _numGroups; }
	int				raidersAlive();

	void tick();
	void onRaiderDied(Raider& raider);
	void stop();

  private:
	Level&				   _level;
	int					   _id;
	BlockPos			   _center;
	int					   _omenLevel, _numGroups, _groupsSpawned = 0;
	Status				   _status			 = Status::Ongoing;
	int					   _ticksActive		 = 0, _cooldownTicks = PRE_RAID_TICKS, _celebrationTicks = 0;
	float				   _waveHealth		 = 0.0F;
	std::vector<EntityRef> _raiders;
	std::unique_ptr<BossBar> _bar;

	bool				   _reattached = false; // Its raiders found again after a load

	void			  spawnGroup();
	std::optional<BlockPos> findSpawnPos();
	float			  healthOfRaiders();
	void			  win();
	void			  lose();
};

// The events of a dimension with villages (ServerLevel's raids, PatrolSpawner, WanderingTraderSpawner)
class VillageEvents {
  public:
	explicit VillageEvents(Level& level) : _level(level) {}
	// Raids.save / the spawners' timers: <dimension>/village_events.json
	void load(const std::filesystem::path& file);
	void save() const;

	Raid* raid(int id);
	Raid* raidAt(const BlockPos& pos);
	// Raids.createOrExtendRaid: a raid at pos (one more omen level if one is going on there)
	Raid* startRaid(const BlockPos& pos, int omenLevel);
	void  onRaiderDied(Raider& raider);
	const std::map<int, std::unique_ptr<Raid>>& raids() const { return _raids; }

	// Each tick: the raids, the omens of the players, the spawners
	void tick(bool spawners);
	// PatrolSpawner.spawnPatrol: a patrol (its leader first) around pos
	int	 spawnPatrol(const BlockPos& pos, int count);
	// WanderingTraderSpawner.spawn: a wandering trader (and two llamas) near the player
	bool spawnWanderingTrader(Player& player);

  private:
	Level&								 _level;
	std::map<int, std::unique_ptr<Raid>> _raids;
	int									 _nextRaidId = 1;
	std::map<int, BlockPos>				 _omenPositions; // Player id -> where its Raid Omen started
	int _patrolTick = 12000, _traderTick = 1200, _traderSpawnDelay = 24000, _traderSpawnChance = 25;
	std::filesystem::path _file;

	void tickOmens();
	void tickPatrolSpawner();
	void tickWanderingTraderSpawner();
	std::optional<BlockPos> surfaceNear(const BlockPos& pos);
};

// CarvedPumpkinBlock.trySpawnGolem: a carved pumpkin (or jack o'lantern) on two snow blocks makes a snow golem, on a
// T of iron blocks an iron golem, on a copper block a copper golem
class CarvedPumpkinBehavior : public BlockBehavior {
  public:
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
};

#endif
