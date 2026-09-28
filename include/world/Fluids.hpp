#ifndef FLUIDS_HPP
#define FLUIDS_HPP

#include "world/BlockPos.hpp"
#include "world/entity/Geometry.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

class GameData;
class Level;

// Fluid state of a block (vanilla's FluidState): water in a water block, in a waterlogged stair...
struct FluidState {
	int	 type	 = 0; // minecraft:fluid id, 0 = empty
	int	 amount	 = 0; // 1-8, 8 for sources and falling fluids
	bool falling = false;

	bool isEmpty() const { return type == 0; }
	bool operator==(const FluidState& other) const { return type == other.type && amount == other.amount && falling == other.falling; }
	bool operator!=(const FluidState& other) const { return !(*this == other); }
};

// Water and lava, ported from vanilla's FlowingFluid, WaterFluid and LavaFluid: how they flow (scheduled fluid
// ticks), find the nearest slope, make infinite sources, turn into stone, cobblestone or obsidian...
class Fluids {
  public:
	Fluids(Level& level, const GameData& gameData, bool ultraWarm);

	FluidState stateOf(int blockState) const;
	bool	   isSource(const FluidState& fluid) const { return fluid.type == _water || fluid.type == _lava; }
	bool	   isWater(int type) const { return type == _water || type == _flowingWater; } // FluidTags.WATER
	bool	   isLava(int type) const { return type == _lava || type == _flowingLava; }	   // FluidTags.LAVA
	// FluidState.randomTick: lava sets fire to what burns around it. Fire isn't ported yet: nothing happens
	void	   randomTick(const BlockPos&, int) {}
	int		   water() const { return _water; }
	// The block the fluid forms on its own (createLegacyBlock): water/lava with the matching level, or air
	int legacyBlock(const FluidState& fluid) const;
	int tickDelay(int type) const;

	// Scheduled fluid tick (FluidState.tick)
	void tick(const BlockPos& pos, int blockState, FluidState fluid);
	// LiquidBlock.shouldSpreadLiquid: lava next to water becomes obsidian or cobblestone (basalt above soul soil, next to
	// blue ice). Returns false if the lava was replaced
	bool shouldSpreadLiquid(const BlockPos& pos, int blockState);
	// SimpleWaterloggedBlock and co. (LiquidBlockContainer): whether the block at this state can take this fluid in
	bool canPlaceLiquid(int blockState, int type) const;
	// FluidState.getHeight: 1 under the same fluid, else amount / 9
	float height(const FluidState& fluid, const BlockPos& pos);
	// FlowingFluid.getFlow: direction the fluid pushes entities, normalized
	Vec3 flow(const BlockPos& pos, const FluidState& fluid);

  private:
	// Per block, what the fluid code checks (instanceof and tags of vanilla)
	enum BlockFlag : uint8_t {
		LIQUID_CONTAINER   = 1,  // LiquidBlockContainer
		SIMPLE_WATERLOGGED = 2,  // SimpleWaterloggedBlock
		NO_FLUID_INSIDE	   = 4,  // Doors, signs, ladder, sugar cane... (canHoldAnyFluid)
		LIQUID_BLOCK	   = 8,  // LiquidBlock
		ICE				   = 16, // IceBlock
	};

	// The fluid being ticked: water or lava
	struct Kind {
		int	 source, flowing;
		bool lava;
	};

	Level&			_level;
	const GameData& _gameData;
	bool			_ultraWarm;
	int				_water, _flowingWater, _lava, _flowingLava;
	int				_waterBlock, _lavaBlock, _air, _stone, _cobblestone, _obsidian, _basalt, _soulSoil, _blueIce;
	int				_levelProperty, _waterlogged;
	std::vector<uint8_t> _blockFlags;
	// canPassThroughWall result per (shape, shape, direction), shapes never change
	std::unordered_map<uint64_t, bool> _wallCache;

	// Lazy caches of one getSpread computation (FlowingFluid.SpreadContext)
	struct SpreadContext {
		BlockPos							 origin;
		std::unordered_map<int16_t, int>	 states;
		std::unordered_map<int16_t, bool>	 holes;
	};

	Kind  kindOf(int type) const;
	bool  sameKind(const Kind& kind, const FluidState& fluid) const { return fluid.type == kind.source || fluid.type == kind.flowing; }
	bool  isSourceOf(const Kind& kind, const FluidState& fluid) const { return fluid.type == kind.source; }
	int	  dropOff(const Kind& kind) const { return kind.lava && !_ultraWarm ? 2 : 1; }
	int	  slopeFindDistance(const Kind& kind) const { return kind.lava && !_ultraWarm ? 2 : 4; }
	bool  canConvertToSource(const Kind& kind) const { return !kind.lava; } // Game rules waterSourceConversion / lavaSourceConversion
	bool  hasFlag(int blockState, BlockFlag flag) const;
	bool  isSolidFace(const Kind& kind, const BlockPos& pos, Direction direction);

	void	   spread(const Kind& kind, const BlockPos& pos, int blockState, const FluidState& fluid);
	void	   spreadToSides(const Kind& kind, const BlockPos& pos, const FluidState& fluid, int blockState);
	FluidState newLiquid(const Kind& kind, const BlockPos& pos, int blockState);
	void	   spreadTo(const Kind& kind, const BlockPos& pos, int blockState, Direction direction, const FluidState& fluid);
	int		   spreadDelay(const Kind& kind, const BlockPos& pos, const FluidState& current, const FluidState& next);
	std::vector<std::pair<Direction, FluidState>> getSpread(const Kind& kind, const BlockPos& pos, int blockState);
	int	 slopeDistance(const Kind& kind, const BlockPos& pos, int depth, Direction from, int blockState, SpreadContext& context);
	int	 contextState(SpreadContext& context, const BlockPos& pos);
	bool isHole(const Kind& kind, SpreadContext& context, const BlockPos& pos);
	bool isWaterHole(const Kind& kind, const BlockPos& pos, int blockState, const BlockPos& below, int belowState);
	int	 sourceNeighborCount(const Kind& kind, const BlockPos& pos);

	bool canPassThroughWall(Direction direction, int blockState, int neighborState);
	bool canMaybePassThrough(const Kind& kind, int blockState, Direction direction, int neighborState, const FluidState& neighborFluid);
	bool canPassThrough(const Kind& kind, int type, int blockState, Direction direction, int neighborState, const FluidState& neighborFluid);
	bool canHoldAnyFluid(int blockState) const;
	bool canHoldFluid(int blockState, int type) const { return canHoldAnyFluid(blockState) && canPlaceLiquid(blockState, type); }
	bool canBeReplacedWith(const FluidState& fluid, const BlockPos& pos, int type, Direction direction);
	void fizz(const BlockPos& pos);
};

#endif
