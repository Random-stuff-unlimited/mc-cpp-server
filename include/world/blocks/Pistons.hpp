#ifndef PISTONS_HPP
#define PISTONS_HPP

#include "world/Level.hpp"
#include "world/blocks/Redstone.hpp"

#include <memory>
#include <vector>

// The ids pistons use
struct PistonIds {
	int piston, stickyPiston, pistonHead, movingPiston, slime, honey, obsidian, cryingObsidian, respawnAnchor, reinforcedDeepslate;
	int extended, type, shortProperty, normal, sticky, waterlogged;
	std::vector<bool> hasBlockEntity; // Per block: an EntityBlock (never pushed)

	explicit PistonIds(const BlockContext& context);
};

// PistonBaseBlock: extends through block events (0 extend, 1 retract, 2 retract dropping the block it was pushing)
class PistonBaseBlock : public RedstoneBehavior {
  public:
	PistonBaseBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, std::shared_ptr<const PistonIds> pistons, bool sticky);

	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	void setPlacedBy(Level& level, const BlockPos& pos, int state) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	bool triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const override;

	// PistonBaseBlock.isPushable
	static bool isPushable(Level& level, const PistonIds& ids, int state, const BlockPos& pos, Direction direction, bool allowDestroy, Direction pistonFacing);

  private:
	std::shared_ptr<const PistonIds> _pistons;
	bool							 _sticky;

	void checkIfExtend(Level& level, const BlockPos& pos, int state) const;
	bool getNeighborSignal(Level& level, const BlockPos& pos, Direction facing) const;
	bool moveBlocks(Level& level, const BlockPos& pos, Direction facing, bool extending) const;
};

// PistonHeadBlock: goes with its base
class PistonHeadBlock : public RedstoneBehavior {
  public:
	PistonHeadBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, std::shared_ptr<const PistonIds> pistons);

	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;

  private:
	std::shared_ptr<const PistonIds> _pistons;
	bool							 isFittingBase(int head, int base) const;
};

// MovingPistonBlock: a block on its way (its PistonMovingBlockEntity is in the level)
class MovingPistonBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
};

// PistonMovingBlockEntity.tick and finalTick, given to the level
void registerMovingPistons(Level& level, std::shared_ptr<const BlockContext> context, std::shared_ptr<const PistonIds> pistons);

#endif
