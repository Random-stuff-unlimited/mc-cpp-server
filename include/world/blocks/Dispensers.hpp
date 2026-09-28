#ifndef DISPENSERS_HPP
#define DISPENSERS_HPP

#include "world/blockentity/ContainerEntities.hpp"
#include "world/blocks/Containers.hpp"
#include "world/blocks/Redstone.hpp"

#include <memory>

// DispenserBlock and DropperBlock: a 3x3 container that fires one item on a rising edge of power (4 ticks later).
// Droppers drop it, or put it in the container in front; dispensers drop it too, except for the items with a
// behavior of their own (buckets for now)
class DispenserBlock : public RedstoneBehavior {
  public:
	DispenserBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool dropper);

	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	int	 getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	std::unique_ptr<BlockEntity> newBlockEntity(const BlockPos& pos, int state) const override;

  private:
	bool _dropper;
	int	 _triggered;
	int	 _bucket, _waterBucket, _lavaBucket, _powderSnowBucket;

	void	  dispenseFrom(Level& level, const BlockPos& pos, int state) const;
	ItemStack dispense(Level& level, const BlockPos& pos, int state, DispenserBlockEntity& dispenser, ItemStack stack) const;
	// DefaultDispenseItemBehavior: one item flies out, with the click sound and the smoke
	ItemStack dispenseDefault(Level& level, const BlockPos& pos, int state, ItemStack stack) const;
	void	  spawnItem(Level& level, const BlockPos& pos, Direction facing, ItemStack stack) const;
	void	  playDefault(Level& level, const BlockPos& pos, Direction facing) const;
	ItemStack consumeWithRemainder(Level& level, const BlockPos& pos, int state, DispenserBlockEntity& dispenser, ItemStack stack,
								   ItemStack remainder) const;
	// BucketItem.emptyContents and BucketPickup.pickupBlock, without a player
	bool	  emptyBucket(Level& level, const BlockPos& at, int fluid) const;
	ItemStack fillBucket(Level& level, const BlockPos& at) const;
};

#endif
