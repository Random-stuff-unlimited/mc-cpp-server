#ifndef SPIDER_HPP
#define SPIDER_HPP

#include "world/entity/mobs/Monster.hpp"

// Spider (and cave spider): climbs walls, leaps at its target, only hunts in the dark (gives up in daylight). Cave
// spiders poison what they bite
class Spider : public Monster {
  public:
	static constexpr int DATA_FLAGS = 16; // 1: climbing

	Spider(Level& level, int typeId) : Monster(level, typeId) {}

	bool isClimbing() const { return _flags & 1; }
	void setClimbing(bool climbing);
	bool onClimbable() override { return isClimbing(); }

	void registerGoals() override;
	void tick() override;
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance& difficulty, int reason, std::shared_ptr<SpawnGroupData> group) override;

  protected:
	std::unique_ptr<PathNavigation> createNavigation() override;
	void							writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const override;

  private:
	uint8_t _flags = 0;
};

class CaveSpider : public Spider {
  public:
	using Spider::Spider;
	bool doHurtTarget(Actor& target) override;
	// CaveSpider.finalizeSpawn: nothing at all (not even Mob's follow range bonus)
	std::shared_ptr<SpawnGroupData> finalizeSpawn(DifficultyInstance&, int, std::shared_ptr<SpawnGroupData> group) override { return group; }
};

#endif
