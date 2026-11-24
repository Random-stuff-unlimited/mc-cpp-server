#ifndef LIGHTNING_BOLT_HPP
#define LIGHTNING_BOLT_HPP

#include "world/entity/Entity.hpp"

#include <set>

// LightningBolt: strikes for a few ticks (1 to 3 flashes), setting fire around where it hit (normal and hard) and
// striking what is within 3 blocks (Entity.thunderHit: 5 damage and fire; creepers get charged, pigs become
// zombified piglins, villagers witches, mooshrooms change color). The client draws it and plays the thunder.
// Thunderstorms make them (ServerLevel.tickThunder), drawn to lightning rods and to the living under the sky
class LightningBolt : public Entity {
  public:
	explicit LightningBolt(Level& level);

	bool isVisualOnly() const { return _visualOnly; }
	void setVisualOnly(bool visualOnly) { _visualOnly = visualOnly; }
	// The block it strikes (getStrikePosition): the one it stands on
	BlockPos strikePosition() const;

	void tick() override;

	// ServerLevel.findLightningTargetAround: the top of the column, or a lightning rod within 128 blocks whose top is
	// the highest block there, or a living thing under the sky around the column
	static BlockPos findTarget(Level& level, const BlockPos& around);
	// ServerLevel.tickThunder for a chunk: during a thunderstorm, 1 chance in 100000 per tick of a bolt where it rains
	static void tickThunder(Level& level, int chunkX, int chunkZ);

  private:
	int			  _life = 2, _flashes;
	bool		  _visualOnly = false;
	std::set<int> _hit; // Ids of what it struck already

	void spawnFire(int extra);
	void powerLightningRod();
};

#endif
