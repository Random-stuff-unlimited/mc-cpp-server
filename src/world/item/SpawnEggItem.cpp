#include "world/item/SpawnEggItem.hpp"

#include "data/GameData.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"

namespace SpawnEggItem {

	Mob* useOn(Level& level, Player* player, ItemStack& stack, const BlockPos& clicked, Direction face) {
		const GameData& data = level.gameData();
		int				type = data.getSpawnEggType(stack.item);
		if (type < 0) return nullptr;
		int block = data.getBlocks().blockOf(level.getBlockState(clicked));
		if (block == data.getStaticId("minecraft:block", "minecraft:spawner") || block == data.getStaticId("minecraft:block", "minecraft:trial_spawner")) {
			return nullptr;
		}
		bool	 empty = data.getCollisionShape(level.getBlockState(clicked)).empty();
		BlockPos at	   = empty ? clicked : clicked.relative(face);
		// spawnMob: monsters don't appear in peaceful
		const GameData::EntityTypeInfo* info = data.getEntityType(type);
		if (!info || (!info->allowedInPeaceful && level.server().getConfig().getDifficulty() == "peaceful")) return nullptr;
		Mob* mob = level.mobs().spawn(level, type, at, MobRegistry::SpawnReason::SpawnItemUse, true, !(at == clicked) && face == Direction::Up);
		// ItemStack.consume: kept in creative (infinite materials)
		if (mob && !(player && player->getGameMode() == GameMode::Creative)) stack.shrink(1);
		return mob;
	}

} // namespace SpawnEggItem
