#ifndef LIQUID_BLOCK_HPP
#define LIQUID_BLOCK_HPP

#include "world/BlockBehavior.hpp"

// Vanilla's LiquidBlock (water, lava)
class LiquidBlock : public BlockBehavior {
  public:
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
};

#endif
