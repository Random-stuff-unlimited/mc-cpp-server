#ifndef PROCESSING_BLOCKS_HPP
#define PROCESSING_BLOCKS_HPP

#include "world/blocks/Containers.hpp"

// Blocks that process items: furnaces and brewing stands (their block entities are in
// world/blockentity/ProcessingEntities.hpp). Their comparator output and neighbor updates when broken come from
// ContainerBlock; furnaces keep their facing placement rule (FacingPlacement), their flames are drawn by the client

// AbstractFurnaceBlock (FurnaceBlock, BlastFurnaceBlock, SmokerBlock): opens its menu; "lit" while it burns
class AbstractFurnaceBlock : public ContainerBlock {
  public:
	using ContainerBlock::ContainerBlock;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
};

// BrewingStandBlock: opens its menu; has_bottle_0 to 2 show its bottles
class BrewingStandBlock : public ContainerBlock {
  public:
	using ContainerBlock::ContainerBlock;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
};

#endif
