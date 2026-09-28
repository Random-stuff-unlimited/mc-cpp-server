#include "world/blocks/LiquidBlock.hpp"

#include "world/Fluids.hpp"
#include "world/Level.hpp"

// Water and lava blocks: they only schedule their fluid tick, Fluids does the rest

void LiquidBlock::onPlace(Level& level, const BlockPos& pos, int state, int, bool) const {
	Fluids& fluids = level.fluids();
	if (fluids.shouldSpreadLiquid(pos, state)) {
		int type = fluids.stateOf(state).type;
		level.scheduleFluidTick(pos, type, fluids.tickDelay(type));
	}
}

void LiquidBlock::neighborChanged(Level& level, const BlockPos& pos, int state, int, bool) const {
	Fluids& fluids = level.fluids();
	if (fluids.shouldSpreadLiquid(pos, state)) {
		int type = fluids.stateOf(state).type;
		level.scheduleFluidTick(pos, type, fluids.tickDelay(type));
	}
}

int LiquidBlock::updateShape(Level& level, const BlockPos& pos, int state, Direction, const BlockPos&, int neighborState) const {
	Fluids&	   fluids = level.fluids();
	FluidState fluid  = fluids.stateOf(state);
	if (fluids.isSource(fluid) || fluids.isSource(fluids.stateOf(neighborState))) level.scheduleFluidTick(pos, fluid.type, fluids.tickDelay(fluid.type));
	return state;
}
