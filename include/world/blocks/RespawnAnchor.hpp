#ifndef RESPAWN_ANCHOR_HPP
#define RESPAWN_ANCHOR_HPP

#include "world/BlockBehavior.hpp"
#include "world/blocks/BlockContext.hpp"

#include <memory>

// RespawnAnchorBlock: charged with glowstone (4 charges), sets the respawn point where anchors work (the nether),
// explodes anywhere else; each respawn there uses a charge (Combat::findRespawnAndUseSpawnBlock)
class RespawnAnchorBlock : public BlockBehavior {
  public:
	explicit RespawnAnchorBlock(std::shared_ptr<const BlockContext> context);
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	bool	  hasAnalogOutputSignal(int) const override { return true; }
	int		  getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_charges, _glowstone;

	void explode(Level& level, const BlockPos& pos) const;
};

#endif
