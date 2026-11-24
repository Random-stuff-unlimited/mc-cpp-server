#include "world/entity/Vehicles.hpp"
#include "world/entity/EndCrystal.hpp"
#include "world/item/ItemUseOn.hpp"

#include "data/GameData.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Portals.hpp"
#include "world/blocks/Fire.hpp"
#include "world/item/ItemDamage.hpp"

namespace {
	// CampfireBlock.canLight, CandleBlock.canLight, CandleCakeBlock.canLight: an unlit one (not under water)
	bool canLight(Level& level, int state) {
		const GameData&		 data	= level.gameData();
		const BlockRegistry& blocks = level.blocks();
		int					 block	= blocks.blockOf(state);
		int					 lit = blocks.property("lit"), waterlogged = blocks.property("waterlogged");
		if (!blocks.has(state, lit) || blocks.getBool(state, lit)) return false;
		if (data.isInTag("minecraft:block", "minecraft:candle_cakes", block)) return true;
		if (data.isInTag("minecraft:block", "minecraft:campfires", block) || data.isInTag("minecraft:block", "minecraft:candles", block)) {
			return blocks.has(state, waterlogged) && !blocks.getBool(state, waterlogged);
		}
		return false;
	}

	Direction horizontalDirection(float yaw) {
		// Entity.getDirection: Direction.fromYRot
		int index = Mth::floor(yaw / 90.0 + 0.5) & 3;
		constexpr Direction BY_2D[4] = {Direction::South, Direction::West, Direction::North, Direction::East};
		return BY_2D[index];
	}
} // namespace

namespace ItemUse {

	Result useOn(Level& level, Player& player, int hand, const BlockHit& hit) {
		ItemStack&		   stack = player.inventory().getMutable(player.handSlot(hand));
		const std::string& name	 = level.gameData().getStaticName("minecraft:item", stack.item);
		int				   state = level.getBlockState(hit.pos);
		JavaRandom&		   random = level.random();

		if (name == "minecraft:minecart" || name == "minecraft:chest_minecart" || name == "minecraft:furnace_minecart" || name == "minecraft:tnt_minecart" ||
			name == "minecraft:hopper_minecart") {
			// MinecartItem.useOn: on a rail only
			return Minecart::place(level, player, stack, hit.pos) ? Result::Success : Result::Fail;
		}
		if (name == "minecraft:end_crystal") {
			// EndCrystalItem.useOn: on obsidian or bedrock, with two free blocks above and nothing in the way
			std::string block = level.gameData().getStaticName("minecraft:block", level.blocks().blockOf(state));
			if (block != "minecraft:obsidian" && block != "minecraft:bedrock") return Result::Fail;
			BlockPos above = hit.pos.above();
			if (!level.blocks().isAir(level.getBlockState(above)) || !level.blocks().isAir(level.getBlockState(above.above()))) return Result::Fail;
			AABB box{static_cast<double>(above.x), static_cast<double>(above.y), static_cast<double>(above.z), above.x + 1.0, above.y + 2.0, above.z + 1.0};
			if (!level.actorsIn(box).empty()) return Result::Fail;
			auto crystal = std::make_unique<EndCrystal>(level, Vec3{above.x + 0.5, static_cast<double>(above.y), above.z + 0.5});
			crystal->setShowBottom(false);
			level.entities().add(std::move(crystal));
			if (player.getGameMode() != GameMode::Creative) stack.shrink(1);
			return Result::Success;
		}
		if (name == "minecraft:flint_and_steel") {
			// FlintAndSteelItem.useOn
			BlockPos target = hit.pos;
			int		 placed;
			if (canLight(level, state)) {
				placed = level.blocks().withBool(state, level.blocks().property("lit"), true);
			} else {
				target = hit.pos.relative(hit.face);
				if (!BaseFireBlock::canBePlacedAt(level, target, horizontalDirection(player.getYaw()))) return Result::Fail;
				placed = BaseFireBlock::getState(level, target);
			}
			level.playSound(&player, target, "minecraft:item.flintandsteel.use", Level::SoundSource::Blocks, 1.0F, random.nextFloat() * 0.4F + 0.8F);
			level.setBlock(target, placed, Level::UPDATE_ALL_IMMEDIATE);
			ItemDamage::hurtAndBreak(level, stack, 1, &player, player.handSlot(hand));
			return Result::Success;
		}
		if (name == "minecraft:fire_charge") {
			// FireChargeItem.useOn
			BlockPos target = hit.pos;
			int		 placed;
			if (canLight(level, state)) {
				placed = level.blocks().withBool(state, level.blocks().property("lit"), true);
			} else {
				target = hit.pos.relative(hit.face);
				if (!BaseFireBlock::canBePlacedAt(level, target, horizontalDirection(player.getYaw()))) return Result::Fail;
				placed = BaseFireBlock::getState(level, target);
			}
			float pitch = (random.nextFloat() - random.nextFloat()) * 0.2F + 1.0F;
			level.playSound(nullptr, target, "minecraft:item.firecharge.use", Level::SoundSource::Blocks, 1.0F, pitch);
			level.setBlock(target, placed, Level::UPDATE_ALL);
			if (!player.isCreative()) stack.shrink(1);
			return Result::Success;
		}
		if (name == "minecraft:ender_eye") {
			// EnderEyeItem.useOn: only into an end portal frame without an eye
			if (!Portals::placeEnderEye(level, hit.pos)) return Result::Pass;
			if (!player.isCreative()) stack.shrink(1);
			return Result::Success;
		}
		return Result::Pass;
	}

} // namespace ItemUse
