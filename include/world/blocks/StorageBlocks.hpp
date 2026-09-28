#ifndef STORAGE_BLOCKS_HPP
#define STORAGE_BLOCKS_HPP

#include "world/blockentity/StorageEntities.hpp"
#include "world/blocks/Redstone.hpp"

#include <array>
#include <memory>
#include <optional>
#include <vector>

// Chiseled bookshelves, decorated pots, jukeboxes, lecterns, crafters and shelves (their block entities are in
// world/blockentity/StorageEntities.hpp)

// SelectableSlotContainer.getHitSlot: the slot (rows x columns on the front face) the hit is in, none if another face
std::optional<int> hitSlot(const BlockHit& hit, Direction facing, int rows, int columns);

class StorageBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	// Containers.updateNeighboursAfterDestroy: comparators read it again
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
};

// ChiseledBookShelfBlock: a book in or out of the slot clicked on its front
class ChiseledBookShelfBlock : public StorageBlock {
  public:
	using StorageBlock::StorageBlock;
	int		  getStateForPlacement(Level& level, const PlaceContext& context) const override;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItemAt(Level& level, const BlockPos& pos, int state, Player& player, const BlockHit& hit) const override;
	int		  getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
};

// DecoratedPotBlock: takes items one by one (wobbling), cracks into its sherds when broken with a tool that breaks pots
class DecoratedPotBlock : public StorageBlock {
  public:
	using StorageBlock::StorageBlock;
	int		  getStateForPlacement(Level& level, const PlaceContext& context) const override;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void	  playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const override;
	int		  getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	bool	  triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const override;
};

// JukeboxBlock: plays the disc put in, gives it back when used again; powers what is around while playing
class JukeboxBlock : public StorageBlock {
  public:
	using StorageBlock::StorageBlock;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	bool	  isSignalSource(int) const override { return true; }
	int		  getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int		  getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
};

// LecternBlock: holds a book open for everyone; turning a page gives a redstone pulse
class LecternBlock : public StorageBlock {
  public:
	using StorageBlock::StorageBlock;
	int		  getStateForPlacement(Level& level, const PlaceContext& context) const override;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	bool	  useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void	  tick(Level& level, const BlockPos& pos, int state) const override;
	void	  affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	bool	  isSignalSource(int) const override { return true; }
	int		  getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int		  getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int		  getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	// tryPlaceBook: a book on an empty lectern (hand: the player's, whose stack gives one)
	static bool tryPlaceBook(Level& level, const BlockPos& pos, int state, Player* player, ItemStack& stack);
};

// CrafterBlock: crafts what its grid holds on a rising edge of power (4 ticks later), and throws it out of its front
class CrafterBlock : public StorageBlock {
  public:
	using StorageBlock::StorageBlock;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	void setPlacedBy(Level& level, const BlockPos& pos, int state) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	int	 getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	std::unique_ptr<BlockEntity> newBlockEntity(const BlockPos& pos, int state) const override;

  private:
	Direction front(int state) const;
	void	  dispenseFrom(Level& level, const BlockPos& pos, int state) const;
	void	  dispenseItem(Level& level, const BlockPos& pos, CrafterBlockEntity& crafter, const ItemStack& stack, int state) const;
};

// ShelfBlock: 3 items swapped with the hand; powered, the shelves side by side (up to 3) swap with the whole hotbar
class ShelfBlock : public StorageBlock {
  public:
	using StorageBlock::StorageBlock;
	int		  getStateForPlacement(Level& level, const PlaceContext& context) const override;
	int		  updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void	  onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void	  neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void	  affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	UseResult useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit& hit) const override;
	int		  getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

	// SideChainPartBlock: the shelves chained with this one, left to right
	std::vector<BlockPos> connectedTo(Level& level, const BlockPos& pos) const;

  private:
	enum class Part { Unconnected, Right, Center, Left };
	Part	  part(int state) const;
	int		  withPart(int state, Part part) const;
	Direction facing(int state) const;
	bool	  isConnectable(int state) const;
	void	  setPart(Level& level, const BlockPos& pos, Part part) const;
	void	  updateNeighborsAfterPoweringDown(Level& level, const BlockPos& pos, int state) const;
	void	  updateSelfAndNeighborsOnPoweringUp(Level& level, const BlockPos& pos, int state, int oldState) const;
	bool	  swapHotbar(Level& level, const BlockPos& pos, Player& player) const;
};

// Components of these block entities for the item they drop and from the item placed (collectImplicitComponents /
// applyImplicitComponents), and their loot table's dynamic drops (a cracked pot's sherds)
namespace StorageItems {
	// False if the block entity isn't one of these
	bool collect(const BlockEntity& entity, ItemStack& stack, const GameData& gameData);
	bool apply(BlockEntity& entity, const ItemStack& stack, const GameData& gameData);
	void dynamicDrops(const BlockEntity& entity, const std::string& name, std::vector<ItemStack>& out, const GameData& gameData);
} // namespace StorageItems

#endif
