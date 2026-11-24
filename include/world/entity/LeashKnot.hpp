#ifndef LEASH_KNOT_HPP
#define LEASH_KNOT_HPP

#include "world/entity/Entity.hpp"

class Player;

// LeashFenceKnotEntity: a lead tied to a fence, holding the mobs a player brought there. It goes with its fence, or
// when hit (the mobs drop their leads)
class LeashKnot : public Entity {
  public:
	LeashKnot(Level& level, const BlockPos& fence);
	explicit LeashKnot(Level& level) : LeashKnot(level, BlockPos{0, 0, 0}) {}

	// LeadItem.bindPlayerMobs: the mobs the player holds within 7 blocks of the fence, tied to its knot (made if
	// needed); how many
	static int bindPlayerMobs(Level& level, Player& player, const BlockPos& fence);

	void tick() override;
	bool interact(Player& player, int hand) override;
	bool hurtServer(const Combat::DamageSource& source, float amount) override;
	bool shouldBeSaved() const override { return true; }
	void save(Buffer& buf) const override;
	void load(Buffer& buf) override;

  private:
	BlockPos fence() const;
};

#endif
