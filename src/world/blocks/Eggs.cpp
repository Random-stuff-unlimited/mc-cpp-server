#include "world/blocks/Eggs.hpp"

#include "world/Fluids.hpp"
#include "world/Level.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"

#include <string>

namespace {
	int blockId(Level& level, int state) { return level.blocks().blockOf(state); }

	// The baby of an egg, at pos (with a bit of spread)
	Mob* hatch(Level& level, const char* type, const BlockPos& pos, double dx = 0.0, double dz = 0.0) {
		Mob* mob = level.mobs().spawn(level, level.gameData().getStaticId("minecraft:entity_type", type), pos, MobRegistry::SpawnReason::Breeding);
		if (!mob) return nullptr;
		mob->setBaby(true);
		mob->setPos({pos.x + 0.5 + dx, static_cast<double>(pos.y), pos.z + 0.5 + dz});
		return mob;
	}

	int intProperty(Level& level, int state, const char* name) {
		std::string value = level.gameData().getProperty(state, name);
		return value.empty() ? 0 : std::stoi(value);
	}
	int withIntProperty(Level& level, int state, const char* name, int value) {
		int next = level.gameData().withProperty(state, name, std::to_string(value));
		return next >= 0 ? next : state;
	}
} // namespace

// ===================== FrogspawnBlock =====================

void FrogspawnBlock::onPlace(Level& level, const BlockPos& pos, int state, int, bool) const {
	// FrogspawnBlock.getFrogspawnHatchDelay: 3600 to 12000 ticks
	level.scheduleTick(pos, blockId(level, state), 3600 + level.random().nextInt(8401));
}

bool FrogspawnBlock::canSurvive(Level& level, const BlockPos& pos, int) const {
	// On still water: water under it, air or more frogspawn above
	BlockPos   below = pos.below();
	FluidState fluid = level.getFluidState(below);
	return level.fluids().isWater(fluid.type) && level.getFluidState(pos).isEmpty();
}

int FrogspawnBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int) const {
	return canSurvive(level, pos, state) ? state : level.gameData().getDefaultBlockState("minecraft:air");
}

void FrogspawnBlock::tick(Level& level, const BlockPos& pos, int state) const {
	if (!canSurvive(level, pos, state)) {
		level.destroyBlock(pos, false);
		return;
	}
	// FrogspawnBlock.hatchFrogspawn: gone, and 2 to 5 tadpoles in the water under it
	level.destroyBlock(pos, false);
	level.playSound(nullptr, pos, "minecraft:block.frogspawn.hatch", Level::SoundSource::Blocks, 1.0F, 1.0F);
	JavaRandom& random = level.random();
	int			count  = 2 + random.nextInt(4);
	for (int i = 0; i < count; i++) {
		double dx = (random.nextDouble() - 0.5) * 0.5, dz = (random.nextDouble() - 0.5) * 0.5;
		if (Mob* tadpole = hatch(level, "minecraft:tadpole", pos.below(), dx, dz)) tadpole->setPos({pos.x + 0.5 + dx, pos.y - 0.6, pos.z + 0.5 + dz});
	}
}

// ===================== SnifferEggBlock =====================

int SnifferEggBlock::hatchDelay(Level& level, const BlockPos& pos) {
	// SnifferEggBlock: 24000 ticks to hatch (12000 on moss), a third of it each stage, give or take 15 seconds
	int	 below	 = blockId(level, level.getBlockState(pos.below()));
	bool boosted = level.gameData().isInTag("minecraft:block", "minecraft:sniffer_egg_hatch_boost", below);
	int	 stage	 = (boosted ? 12000 : 24000) / 3;
	return stage + level.random().nextInt(601) - 300;
}

void SnifferEggBlock::onPlace(Level& level, const BlockPos& pos, int state, int, bool) const {
	bool boosted = level.gameData().isInTag("minecraft:block", "minecraft:sniffer_egg_hatch_boost", blockId(level, level.getBlockState(pos.below())));
	if (boosted) level.levelEvent(nullptr, 3009, pos, 0); // The happy particles of a boosted egg
	level.scheduleTick(pos, blockId(level, state), hatchDelay(level, pos));
}

void SnifferEggBlock::tick(Level& level, const BlockPos& pos, int state) const {
	int hatchLevel = intProperty(level, state, "hatch");
	if (hatchLevel < 2) {
		level.playSound(nullptr, pos, "minecraft:block.sniffer_egg.crack", Level::SoundSource::Blocks, 0.7F, 0.9F + level.random().nextFloat() * 0.2F);
		level.setBlock(pos, withIntProperty(level, state, "hatch", hatchLevel + 1), Level::UPDATE_CLIENTS);
		level.scheduleTick(pos, blockId(level, state), hatchDelay(level, pos));
		return;
	}
	level.playSound(nullptr, pos, "minecraft:block.sniffer_egg.hatch", Level::SoundSource::Blocks, 0.7F, 0.9F + level.random().nextFloat() * 0.2F);
	level.destroyBlock(pos, false);
	hatch(level, "minecraft:sniffer", pos);
}
