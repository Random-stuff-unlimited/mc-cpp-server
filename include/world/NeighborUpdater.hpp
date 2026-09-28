#ifndef NEIGHBOR_UPDATER_HPP
#define NEIGHBOR_UPDATER_HPP

#include "world/BlockPos.hpp"

#include <cstdint>
#include <vector>

// What the updates act on (the Level; tests use a fake one)
class NeighborUpdateTarget {
  public:
	virtual ~NeighborUpdateTarget() = default;
	virtual int getBlockState(const BlockPos& pos) = 0;
	// BlockState.handleNeighborChanged: the block at pos reacts to a change caused by sourceBlock
	virtual void executeNeighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston) = 0;
	// NeighborUpdater.executeShapeUpdate: the block at pos adapts its shape to neighborState, in `direction`
	virtual void executeShapeUpdate(Direction direction, const BlockPos& pos, const BlockPos& neighborPos, int neighborState, int flags, int limit) = 0;
};

// Vanilla's CollectingNeighborUpdater: neighbor and shape updates run depth first, but from a stack instead of
// recursion. An update requested while another runs is collected, then run before the rest of the current batch
// (the six updates of updateNeighborsAt). This order is what most redstone behavior depends on.
// After maxChainedUpdates updates in one chain, the rest is skipped (server.properties max-chained-neighbor-updates).
class NeighborUpdater {
  public:
	explicit NeighborUpdater(NeighborUpdateTarget& target, int maxChainedUpdates = 1000000) : _target(target), _maxChained(maxChainedUpdates) {}

	void shapeUpdate(Direction direction, int neighborState, const BlockPos& pos, const BlockPos& neighborPos, int flags, int limit);
	// The state at pos is read when the update runs
	void neighborChanged(const BlockPos& pos, int sourceBlock);
	// With the state already known
	void neighborChanged(int state, const BlockPos& pos, int sourceBlock, bool movedByPiston);
	// The six neighbors in UPDATE_ORDER, but `skip` (none if null)
	void updateNeighborsAtExceptFromFacing(const BlockPos& pos, int sourceBlock, const Direction* skip);

  private:
	struct Update {
		enum class Kind : uint8_t { Shape, Simple, Full, Multi } kind;
		BlockPos  pos;
		BlockPos  neighborPos;	 // Shape
		int		  state	   = 0;	 // Full: state of pos. Shape: neighbor's state
		int		  block	   = 0;	 // Source block
		int		  flags	   = 0;	 // Shape
		int		  limit	   = 0;	 // Shape
		Direction direction = Direction::Down; // Shape
		bool	  movedByPiston = false;
		int8_t	  skip	   = -1; // Multi: direction not updated
		uint8_t	  index	   = 0;	 // Multi: next direction in UPDATE_ORDER
	};

	NeighborUpdateTarget& _target;
	int					  _maxChained;
	std::vector<Update>	  _stack; // Top at the back
	std::vector<Update>	  _addedThisLayer;
	int					  _count = 0;

	void addAndRun(const Update& update);
	void runUpdates();
	// Runs the next step of an update. Returns true if it has more
	bool runNext(Update& update);
};

#endif
