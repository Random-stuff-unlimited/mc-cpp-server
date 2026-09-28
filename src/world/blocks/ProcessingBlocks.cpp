#include "world/blocks/ProcessingBlocks.hpp"

#include "world/Level.hpp"
#include "world/inventory/ProcessingMenus.hpp"

bool AbstractFurnaceBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	if (auto* furnace = dynamic_cast<AbstractFurnaceBlockEntity*>(entity.get())) {
		Menus::openFurnace(player, level, std::shared_ptr<AbstractFurnaceBlockEntity>(entity, furnace));
	}
	return true;
}

bool BrewingStandBlock::useWithoutItem(Level& level, const BlockPos& pos, int, Player& player) const {
	std::shared_ptr<BlockEntity> entity = level.getSharedBlockEntity(pos);
	if (auto* stand = dynamic_cast<BrewingStandBlockEntity*>(entity.get())) {
		Menus::openBrewingStand(player, level, std::shared_ptr<BrewingStandBlockEntity>(entity, stand));
	}
	return true;
}
