#ifndef ITEM_USE_HPP
#define ITEM_USE_HPP

#include "world/BlockPos.hpp"
#include "world/Clip.hpp"
#include "world/entity/Geometry.hpp"
#include "world/item/ItemStack.hpp"

#include <string>

class GameData;
class Level;
class Player;
class Server;

// Right click in the air (Use Item packet) and items used over time: vanilla's ServerPlayerGameMode.useItem,
// ItemStack.use / finishUsingItem / releaseUsing, Item.use and its subclasses that work without entities (Consumable,
// Equippable, BlocksAttacks, BucketItem, BottleItem, SpyglassItem), and LivingEntity.startUsingItem / updatingUsingItem /
// completeUsingItem / releaseUsingItem / stopUsingItem. Game thread only.
//
// Items whose use needs entities that don't exist yet do nothing (Item.use returns PASS), see useWithoutEntities:
// snowballs, eggs, ender pearls, ender eyes, splash / lingering potions, bottles o' enchanting, wind charges, bows,
// crossbows, tridents, fishing rods, firework rockets (elytra boost), boats, spawn eggs, lily pads (PlaceOnWaterBlockItem),
// goat horns, empty maps, books, bundles, knowledge books, carrot / warped fungus on a stick
namespace ItemUse {
	// InteractionResult (SUCCESS and CONSUME both are Success in vanilla)
	enum class Result { Pass, Fail, Consume, Success };

	// ServerboundUseItemPacket: hand (0 main, 1 offhand), after the rotation was applied
	Result useItem(Level& level, Player& player, int hand);
	// ItemUtils.createFilledResult: one of `stack` becomes `result` (a bucket milked), the rest stays in hand; returns
	// what the hand holds then
	ItemStack filledResult(Level& level, Player& player, ItemStack stack, ItemStack result);
	// ServerboundPlayerActionPacket RELEASE_USE_ITEM
	void releaseUsingItem(Level& level, Player& player);
	// Every tick, in LivingEntity.tick: the used item's tick, completed when its time is over
	void updatingUsingItem(Level& level, Player& player);
	void startUsingItem(Player& player, int hand, const GameData& gameData);
	void stopUsingItem(Player& player);

	// EnchantmentHelper.getItemEnchantmentLevel: level of an enchantment ("minecraft:respiration") on a stack, 0 if none
	// or if its components can't be read
	int enchantmentLevel(const GameData& gameData, const ItemStack& stack, const std::string& enchantment);

	// Item.getUseDuration: ticks of use, 0 for items used at once
	int getUseDuration(const GameData& gameData, const ItemStack& stack);

	// BlockHitResult of a ray cast (Level.clip with ClipContext.Block.OUTLINE)
	using HitResult = Clip::HitResult;
	// Level.clip from `from` to `to` through the outline shapes, and the fluid sources if sourceFluids
	// (ClipContext.Fluid.SOURCE_ONLY, else NONE)
	HitResult clip(Level& level, const Vec3& from, const Vec3& to, bool sourceFluids);
	// Item.getPlayerPOVHitResult: from the eyes, where the player looks, within its block interaction range
	HitResult playerPOVHitResult(Level& level, const Player& player, bool sourceFluids);
} // namespace ItemUse

#endif
