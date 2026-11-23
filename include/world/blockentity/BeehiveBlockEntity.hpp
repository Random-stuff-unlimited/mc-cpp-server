#ifndef BEEHIVE_BLOCK_ENTITY_HPP
#define BEEHIVE_BLOCK_ENTITY_HPP

#include "world/blockentity/BlockEntity.hpp"

#include <vector>

class Bee;

// BeehiveBlockEntity (beehives and bee nests): up to 3 bees inside. They stay 30 seconds (2 minutes with nectar),
// longer at night and in the rain, then come out of its front; a bee that brought nectar raises the honey level.
// Broken, it lets them all out, angry at the player around
class BeehiveBlockEntity : public BlockEntity {
  public:
	static constexpr int MAX_OCCUPANTS = 3, MIN_TICKS_NECTAR = 2400, MIN_TICKS_NO_NECTAR = 600;
	enum class ReleaseStatus { HoneyDelivered, BeeReleased, Emergency };

	explicit BeehiveBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:beehive", pos) {}

	bool isFull() const { return _occupants.size() >= MAX_OCCUPANTS; }
	int	 occupantCount() const { return static_cast<int>(_occupants.size()); }
	// BeehiveBlockEntity.addOccupant: the bee goes in (it leaves the level, its data kept here)
	bool addOccupant(Bee& bee);
	// emptyAllLivingFromHive: every bee out (the hive is broken), angry at `player` if there is one
	void releaseAll(Level& level, Player* player);

	bool ticks() const override { return true; }
	void tick(Level& level) override;
	void preRemoveSideEffects(Level& level) override;
	void save(BlockEntityWriter& out) const override;
	void load(BlockEntityReader& in) override;

  private:
	struct Occupant {
		std::vector<uint8_t> data; // Mob::save
		int					 ticksInHive = 0, minTicksInHive = 0;
		bool				 hasNectar = false;
		int					 typeId	   = -1;
	};
	std::vector<Occupant> _occupants;

	bool releaseOccupant(Level& level, Occupant& occupant, ReleaseStatus status, Player* angryAt = nullptr);
};

#endif
