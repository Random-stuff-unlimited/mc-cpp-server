#ifndef ATTACHED_HPP
#define ATTACHED_HPP

#include "world/BlockBehavior.hpp"
#include "world/blocks/BlockContext.hpp"

#include <memory>
#include <vector>

// Blocks held by a neighbor, which break when it can't hold them anymore. Ported from their vanilla classes

class AttachedBlock : public BlockBehavior {
  public:
	explicit AttachedBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)), _air(_context->defaultState("minecraft:air")) {}

  protected:
	std::shared_ptr<const BlockContext> _context;
	int									_air;
};

// BaseTorchBlock (torches, redstone torch): needs the block below to hold its center
class TorchBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// WallTorchBlock, RedstoneWallTorchBlock: needs the sturdy side of the block behind
class WallTorchBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// LanternBlock: standing, or hanging under a block
class LanternBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// LadderBlock
class LadderBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// CarpetBlock: anything but air below
class CarpetBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// SnowLayerBlock
class SnowLayerBlock : public AttachedBlock {
  public:
	explicit SnowLayerBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	// Melts in block light above 11
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::vector<bool> _cannotSurviveOn, _canSurviveOn;
	int				  _snow;

  public:
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// SugarCaneBlock and CactusBlock: break one tick after losing their support (a scheduled tick)
class SugarCaneBlock : public AttachedBlock {
  public:
	explicit SugarCaneBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	// Grows one block every 16 random ticks, up to 3 blocks
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::vector<bool> _dirt, _sand;
	int				  _frostedIce;

  public:
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

class CactusBlock : public AttachedBlock {
  public:
	explicit CactusBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	// Grows like sugar cane; at age 8 it may get a flower instead
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::vector<bool> _sand;
	int				  _cactusFlower;

  public:
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// DoorBlock: the two halves follow each other; the lower one needs a sturdy block below
class DoorBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
};

// BedBlock: the foot and the head go together
class BedBlock : public AttachedBlock {
  public:
	using AttachedBlock::AttachedBlock;
	int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
};

#endif
