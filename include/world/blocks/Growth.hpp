#ifndef GROWTH_HPP
#define GROWTH_HPP

#include "world/BlockBehavior.hpp"
#include "world/blocks/BlockContext.hpp"

#include <memory>
#include <vector>

// Blocks that change by themselves with random ticks (besides the vegetation of Vegetation.hpp): growing plants,
// grass, farmland, leaves, ice, copper...

// CocoaBlock: on a jungle log, grows to age 2
class CocoaBlock : public BlockBehavior {
  public:
	explicit CocoaBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	std::vector<bool>					_jungleLogs;

  public:
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// GrowingPlantHeadBlock and GrowingPlantBodyBlock: kelp, weeping, twisting and cave vines. The head grows in its
// direction; a head with something of the plant after it becomes body, a body with nothing after it becomes head
class GrowingPlantBlock : public BlockBehavior {
  public:
	enum class Kind { Kelp, WeepingVines, TwistingVines, CaveVines };

	GrowingPlantBlock(std::shared_ptr<const BlockContext> context, Kind kind, bool head);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	Kind								_kind;
	bool								_isHead;
	int									_head, _body;
	Direction							_growth;
	double								_probability; // Per random tick, for the head
	bool								_fluidTicks;  // Kelp: its water flows again on updates
	int									_magma, _water, _berries;

	bool canGrowInto(int state) const;

  public:
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;

  private:
	bool isPlant(int state) const { return _context->is(state, _head) || _context->is(state, _body); }
	// Cave vines keep their berries when changing between head and body
	int	 convert(int from, int to) const;
};

// SnowyDirtBlock (podzol, grass, mycelium): snowy under snow. SpreadingSnowyDirtBlock (grass, mycelium): turns
// to dirt when covered, spreads to dirt around in light 9 or more
class SnowyDirtBlock : public BlockBehavior {
  public:
	SnowyDirtBlock(std::shared_ptr<const BlockContext> context, bool spreading);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	bool								_spreading;
	int									_snowy, _dirt, _snowBlock;
	std::vector<bool>					_snowTag;

	bool canBeGrass(Level& level, const BlockPos& pos, int state) const;
};

// NyliumBlock: back to netherrack when covered
class NyliumBlock : public BlockBehavior {
  public:
	explicit NyliumBlock(std::shared_ptr<const BlockContext> context);
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_netherrack;
};

// FarmBlock: wet within 4 blocks of water, dries out then turns to dirt; turns to dirt under a solid block
class FarmBlock : public BlockBehavior {
  public:
	explicit FarmBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_moisture, _dirt;
	std::vector<bool>					_maintainsFarmland;

  public:
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
};

// LeavesBlock: distance to the nearest log (1 to 7), updated one tick after a neighbor changes; at 7 and not
// placed by a player, they decay
class LeavesBlock : public BlockBehavior {
  public:
	explicit LeavesBlock(std::shared_ptr<const BlockContext> context);
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_distance, _persistent;
	std::vector<bool>					_logs, _leaves;

	int distanceAt(int state) const;
};

// IceBlock: melts in block light
class IceBlock : public BlockBehavior {
  public:
	explicit IceBlock(std::shared_ptr<const BlockContext> context);
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_water;
};

// RedStoneOreBlock: stops glowing
class RedStoneOreBlock : public BlockBehavior {
  public:
	explicit RedStoneOreBlock(std::shared_ptr<const BlockContext> context);
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_lit;
};

// BuddingAmethystBlock: grows amethyst buds on its sides, AmethystClusterBlock: they need the block behind them
class BuddingAmethystBlock : public BlockBehavior {
  public:
	explicit BuddingAmethystBlock(std::shared_ptr<const BlockContext> context);
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	int									_small, _medium, _large, _cluster, _water;
};

class AmethystClusterBlock : public BlockBehavior {
  public:
	explicit AmethystClusterBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)) {}
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
};

// WeatheringCopper (ChangeOverTimeBlock): oxidizes slowly, not before the less oxidized copper around. Only its
// random tick: registered with BlockBehaviors::setRandomTick, the block keeps its own behavior (doors, bulbs...)
class WeatheringBlock : public BlockBehavior {
  public:
	// ages: per block, its WeatherState (0 unaffected .. 3 oxidized), -1 if not weathering copper
	WeatheringBlock(std::shared_ptr<const BlockContext> context, int next, std::shared_ptr<const std::vector<int8_t>> ages)
		: _context(std::move(context)), _next(next), _ages(std::move(ages)) {}
	void randomTick(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const BlockContext>		  _context;
	int										  _next; // Next block, -1 if fully oxidized
	std::shared_ptr<const std::vector<int8_t>> _ages;
};

#endif
