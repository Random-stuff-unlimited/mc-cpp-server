#include "world/blocks/RespawnAnchor.hpp"

#include "network/server.hpp"
#include "player.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"

namespace {
	// The anchor's explosion near water: vanilla's calculator gives every block the water's resistance (its position
	// check compares the position with itself), so nothing breaks
	class WaterCalculator : public Explosions::Calculator {
	  public:
		std::optional<float> blockExplosionResistance(Level&, const BlockPos&, int, const FluidState&) const override { return 100.0F; }
	};
	const WaterCalculator WATER_CALCULATOR;

	// RespawnAnchorBlock.isWaterThatWouldFlow
	bool isWaterThatWouldFlow(Level& level, const BlockPos& pos) {
		FluidState fluid = level.getFluidState(pos);
		if (!level.fluids().isWater(fluid.type)) return false;
		if (level.fluids().isSource(fluid)) return true;
		if (fluid.amount < 2) return false;
		return !level.fluids().isWater(level.getFluidState(pos.below()).type);
	}
} // namespace

RespawnAnchorBlock::RespawnAnchorBlock(std::shared_ptr<const BlockContext> context)
	: _context(std::move(context)), _charges(_context->blocks.property("charges")),
	  _glowstone(_context->data.getStaticId("minecraft:item", "minecraft:glowstone")) {}

UseResult RespawnAnchorBlock::useItemOn(Level& level, const BlockPos& pos, int state, Player& player, int hand, const BlockHit&) const {
	int	 charges	= _context->blocks.getInt(state, _charges);
	bool chargeable = charges < 4;
	if (player.getItemInHand(hand) == _glowstone && chargeable) {
		// RespawnAnchorBlock.charge
		level.setBlock(pos, _context->blocks.withInt(state, _charges, charges + 1), Level::UPDATE_ALL);
		level.playSoundAt(nullptr, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, "minecraft:block.respawn_anchor.charge", Level::SoundSource::Blocks, 1.0F, 1.0F);
		if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
		return UseResult::Success;
	}
	// Glowstone in the other hand: that hand charges it
	if (hand == 0 && player.getItemInHand(1) == _glowstone && chargeable) return UseResult::Pass;
	return UseResult::TryWithEmptyHand;
}

bool RespawnAnchorBlock::useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (_context->blocks.getInt(state, _charges) == 0) return false;
	if (!level.dimensionType().respawnAnchorWorks) {
		explode(level, pos);
		return true;
	}
	PlayerSpawn home{true, pos.x, pos.y, pos.z, level.dimensionName(), false};
	PlayerSpawn& current = player.spawn();
	bool same = current.valid && current.x == home.x && current.y == home.y && current.z == home.z && current.dimension == home.dimension;
	if (!same) {
		current = home;
		level.server().sendSystemMessage(player, "block.minecraft.set_spawn");
		level.playSoundAt(nullptr, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, "minecraft:block.respawn_anchor.set_spawn", Level::SoundSource::Blocks, 1.0F, 1.0F);
	}
	return true;
}

void RespawnAnchorBlock::explode(Level& level, const BlockPos& pos) const {
	level.removeBlock(pos, false);
	bool water = level.fluids().isWater(level.getFluidState(pos.above()).type);
	for (Direction direction : Directions::HORIZONTAL) water = water || isWaterThatWouldFlow(level, pos.relative(direction));
	Vec3 center{pos.x + 0.5, pos.y + 0.5, pos.z + 0.5};
	Explosions::explode(level, nullptr, Combat::DamageSource{"minecraft:bad_respawn_point", nullptr, nullptr, center}, water ? &WATER_CALCULATOR : nullptr,
						center, 5.0F, true, Explosions::Interaction::Block);
}

int RespawnAnchorBlock::getAnalogOutputSignal(Level&, const BlockPos&, int state, Direction) const {
	return Mth::floor(_context->blocks.getInt(state, _charges) / 4.0F * 15); // getScaledChargeLevel
}
