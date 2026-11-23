#include "world/entity/ai/CrossbowAttack.hpp"

#include "world/Level.hpp"

bool isCrossbow(Level& level, const ItemStack& stack) {
	return !stack.isEmpty() && stack.item == level.gameData().getStaticId("minecraft:item", "minecraft:crossbow");
}

void playCrossbowLoaded(Mob& mob) {
	mob.level().playSoundAt(nullptr, mob.position().x, mob.position().y, mob.position().z, "minecraft:item.crossbow.loading_end", Level::SoundSource::Hostile,
							1.0F, 1.0F);
}
