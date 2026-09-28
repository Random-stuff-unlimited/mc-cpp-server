#ifndef CONTAINER_BLOCKS_HPP
#define CONTAINER_BLOCKS_HPP

#include "world/blockentity/ContainerEntities.hpp"
#include "world/blocks/Growth.hpp"
#include "world/blocks/Redstone.hpp"

#include <memory>
#include <string>
#include <vector>

// Blocks holding items: chests, barrels, shulker boxes, ender chests, hoppers (their block entities are in
// world/blockentity/ContainerEntities.hpp)

// The container's fullness for comparators (AbstractContainerMenu.getRedstoneSignalFromContainer)
int redstoneSignalFromContainer(Container& container, const GameData& gameData);

class ContainerBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	// Containers.updateNeighboursAfterDestroy: comparators read it again
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	int	 getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
};

// ChestBlock, TrappedChestBlock, CopperChestBlock: single or double (joined with a chest of the same kind beside it,
// facing the same way)
class ChestBlock : public ContainerBlock {
  public:
	enum class Kind { Chest, Trapped, Copper };
	ChestBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, Kind kind);

	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	bool triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const override { return type == 1; }
	int	 getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	bool keepsBlockEntityOf(int oldState) const override;
	bool isSignalSource(int) const override { return _kind == Kind::Trapped; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	Kind			  _kind;
	int				  _type, _single, _left, _right;
	std::vector<bool> _copperChests;

	bool	  canConnectTo(int self, int other) const;
	Direction connectedDirection(int state) const;
	// candidatePartnerFacing: the facing of a single chest of the same kind on that side, if any
	bool	  candidatePartnerFacing(Level& level, const BlockPos& pos, Direction side, int self, Direction& facing) const;
	// The double chest, or the chest alone, as one container (nullptr if blocked)
	std::shared_ptr<Container> container(Level& level, const BlockPos& pos, bool ignoreBlocked, std::vector<uint8_t>* title, bool* isDouble) const;
};

// WeatheringCopperChestBlock.randomTick: oxidizes like copper, but only the left or single half, while nobody has it
// open (the right half follows through updateShape)
class CopperChestWeathering : public WeatheringBlock {
  public:
	using WeatheringBlock::WeatheringBlock;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;
};

// BarrelBlock: faces where placed from, "open" while someone looks inside
class BarrelBlock : public ContainerBlock {
  public:
	using ContainerBlock::ContainerBlock;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
};

// ShulkerBoxBlock: faces the clicked face; opens only if its lid has room; keeps its items when broken
class ShulkerBoxBlock : public ContainerBlock {
  public:
	using ContainerBlock::ContainerBlock;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	bool triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const override;
	void playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const override;
};

// EnderChestBlock: the player's own ender chest items
class EnderChestBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	bool triggerEvent(Level& level, const BlockPos& pos, int state, int type, int data) const override { return type == 1; }
};

// HopperBlock: enabled unless powered; pulls item entities in
class HopperBlock : public ContainerBlock {
  public:
	using ContainerBlock::ContainerBlock;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void entityInside(Level& level, const BlockPos& pos, int state, Entity* entity) const override;

  private:
	void checkPoweredState(Level& level, const BlockPos& pos, int state) const;
};

// CraftingTableBlock: opens a 3x3 crafting grid (the crafting itself isn't there yet)
class CraftingTableBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
};

// The components a container's block entity gives the item it drops (custom_name, container), and the ones it takes
// from the item placed (BlockEntity.collectComponents / applyComponentsFromItemStack)
namespace ContainerItems {
	void collect(const BlockEntity& entity, ItemStack& stack, const GameData& gameData);
	void apply(BlockEntity& entity, const ItemStack& stack, const GameData& gameData);
} // namespace ContainerItems

#endif
