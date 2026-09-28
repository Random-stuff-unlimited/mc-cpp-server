#ifndef BLOCK_BEHAVIOR_HPP
#define BLOCK_BEHAVIOR_HPP

#include "world/BlockPos.hpp"

#include <memory>
#include <vector>

class BlockEntity;
class Entity;
class Level;
class Player;
struct PlaceContext;

// BlockBehavior::getStateForPlacement when the block has no placement rule of its own yet: the server's generic
// placement (axis, facing, two-block shapes) decides
constexpr int GENERIC_PLACEMENT = -2;

// What a block does, like vanilla's Block methods. States are block state ids; the default does nothing, so only
// blocks with a behavior (redstone, plants, fluids...) override anything. One instance per block, shared by all
// its states: behaviors keep no per-position data.
class BlockBehavior {
  public:
	virtual ~BlockBehavior() = default;

	// Scheduled tick (Level::scheduleTick)
	virtual void tick(Level& level, const BlockPos& pos, int state) const;
	// Random ticks (plants growing...): only called for states where isRandomlyTicking is true
	virtual bool isRandomlyTicking(int state) const;
	virtual void randomTick(Level& level, const BlockPos& pos, int state) const;

	// A neighbor changed (neighborChanged): sourceBlock is the block that caused the update
	virtual void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const;
	// The neighbor in `direction` is now neighborState: returns this block's new state (air = it breaks)
	virtual int updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const;
	// Diagonal shape updates (redstone wire)
	virtual void updateIndirectNeighbourShapes(Level& level, const BlockPos& pos, int state, int flags, int limit) const;

	// The block was just placed (oldState was there before)
	virtual void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const;
	// The block was just replaced by another one: updates what it was powering...
	virtual void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const;
	// Block event (Level::blockEvent), run at the end of the tick: pistons, note blocks... Returns true to send it
	// to the clients
	virtual bool triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const;

	// ----- Redstone (SignalGetter) -----

	// Gives power to what is around it
	virtual bool isSignalSource(int state) const;
	// Power toward the block on its side opposite to `direction` (direction: from that block to this one)
	virtual int getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const;
	// Strong power, through the block it is attached to
	virtual int getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const;
	// Comparators read it (containers, cake...)
	virtual bool hasAnalogOutputSignal(int state) const;
	virtual int	 getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const;

	// ----- Players and entities -----

	// The state for a player placing it (-1: it can't be placed there, GENERIC_PLACEMENT: no rule of its own)
	virtual int getStateForPlacement(Level& level, const PlaceContext& context) const;
	// Whether it can stay at pos (placement checks it, most blocks check it in updateShape)
	virtual bool canSurvive(Level& level, const BlockPos& pos, int state) const;
	// Just placed by a player (Block.setPlacedBy)
	virtual void setPlacedBy(Level& level, const BlockPos& pos, int state) const;
	// Right-clicked with nothing it uses (useWithoutItem): true if it did something
	virtual bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const;
	// An entity is in its cell (pressure plates, hoppers...); nullptr for a player
	virtual void entityInside(Level& level, const BlockPos& pos, int state, Entity* entity) const;
	// A player is about to break it (Block.playerWillDestroy)
	virtual void playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const;
	// shouldChangedStateKeepBlockEntity: this block keeps the block entity of the one it replaces (copper chests
	// oxidizing)
	virtual bool keepsBlockEntityOf(int oldState) const;
	// EntityBlock.newBlockEntity: the block entity that comes with the block, nullptr if none
	virtual std::unique_ptr<BlockEntity> newBlockEntity(const BlockPos& pos, int state) const;
};

// The behavior of every block, by minecraft:block registry id
class BlockBehaviors {
  public:
	explicit BlockBehaviors(size_t blockCount);
	BlockBehaviors(const BlockBehaviors&)			  = delete; // Holds pointers to its own members
	BlockBehaviors& operator=(const BlockBehaviors&) = delete;

	void set(int block, std::unique_ptr<BlockBehavior> behavior);
	const BlockBehavior& get(int block) const { return *_byBlock[block]; }
	// A placement rule of its own for a block whose behavior has none (dispensers, furnaces... face the player)
	void setPlacement(int block, std::unique_ptr<BlockBehavior> behavior);
	const BlockBehavior& placer(int block) const { return _placers[block] ? *_placers[block] : *_byBlock[block]; }
	// A random tick of its own for a block (copper that oxidizes, whatever its kind of block)
	void setRandomTick(int block, std::unique_ptr<BlockBehavior> behavior);
	const BlockBehavior& randomTicker(int block) const { return _randomTickers[block] ? *_randomTickers[block] : *_byBlock[block]; }

  private:
	BlockBehavior								_default;
	std::vector<std::unique_ptr<BlockBehavior>> _owned;
	std::vector<const BlockBehavior*>			_byBlock;
	std::vector<const BlockBehavior*>			_randomTickers;
	std::vector<const BlockBehavior*>			_placers;
};

#endif
