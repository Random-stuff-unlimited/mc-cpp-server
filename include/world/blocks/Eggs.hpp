#ifndef EGGS_HPP
#define EGGS_HPP

#include "world/BlockBehavior.hpp"

// FrogspawnBlock: frog eggs on still water; after 10 to 20 minutes they hatch into 2 to 5 tadpoles (and they go if
// the water under them does)
class FrogspawnBlock : public BlockBehavior {
  public:
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
};

// SnifferEggBlock: cracks twice and hatches a snifflet, in 20 minutes (10 on moss)
class SnifferEggBlock : public BlockBehavior {
  public:
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;

  private:
	static int hatchDelay(Level& level, const BlockPos& pos);
};

#endif
