#ifndef BEACON_BLOCK_ENTITY_HPP
#define BEACON_BLOCK_ENTITY_HPP

#include "world/blockentity/BlockEntity.hpp"
#include "world/entity/MobEffects.hpp"

#include <optional>

// BeaconBlockEntity: every 4 seconds, the pyramid under it (1 to 4 levels of #beacon_base_blocks) and its beam (no
// opaque block above, bedrock aside) decide whether it gives its powers to the players around it: the primary one
// (amplified when it is also the secondary, at 4 levels) and, at 4 levels, regeneration as the secondary
class BeaconBlockEntity : public BlockEntity {
  public:
	using Power = std::optional<MobEffects::Effect>;
	explicit BeaconBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:beacon", pos) {}

	int	  levels() const { return _levels; }
	bool  hasBeam() const { return _beam; }
	Power primary() const { return _primary; }
	Power secondary() const { return _secondary; }
	// ContainerData.set of the beacon menu: filterEffect keeps only the beacon's powers
	void  setPowers(Power primary, Power secondary);
	// BEACON_EFFECTS: the powers of each level (the 4th is the secondary's)
	static bool isValidPower(MobEffects::Effect effect);

	bool ticks() const override { return true; }
	void tick(Level& level) override;
	void save(BlockEntityWriter& out) const override;
	void load(BlockEntityReader& in) override;
	void writeUpdateTag(std::vector<uint8_t>& out) const override;
	bool hasUpdatePacket() const override { return true; }

	// updateBase: the levels of the pyramid under pos (0 to 4)
	static int pyramidLevels(Level& level, const BlockPos& pos);

  private:
	int	  _levels = 0;
	bool  _beam	  = false;
	Power _primary, _secondary;

	bool checkBeam(Level& level) const;
	void applyEffects(Level& level) const;
};

#endif
