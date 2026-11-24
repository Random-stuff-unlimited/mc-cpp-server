#ifndef RAILS_HPP
#define RAILS_HPP

#include "world/BlockBehavior.hpp"

#include <string>

// BaseRailBlock (rails, powered rails, detector rails, activator rails), simplified from vanilla's RailState: a rail
// connects to the rails next to it (curving for plain rails, sloping up to a rail a block higher), breaks without
// support. Powered and activator rails are on with a redstone signal next to them, or through a line of up to 8 rails
// of their kind from one that has it. A detector rail gives a signal while a minecart is on it
class RailBehavior : public BlockBehavior {
  public:
	enum class Kind { Plain, Powered, Detector, Activator };

	explicit RailBehavior(Kind kind) : _kind(kind) {}

	// The rail block at pos (any kind), whether it is one
	static bool isRail(Level& level, const BlockPos& pos);
	// The shape of a rail state ("north_south", "ascending_east"...)
	static std::string shape(Level& level, int state);
	// DetectorRailBlock.checkPressed: a minecart is on it (called by the minecart)
	static void minecartOn(Level& level, const BlockPos& pos);

	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	bool isSignalSource(int) const override { return _kind == Kind::Detector; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	Kind _kind;

	// RailState.place: the shape that connects it to the rails around
	void connect(Level& level, const BlockPos& pos, bool neighborsToo) const;
	// PoweredRailBlock.updateState: powered by a signal, or by its line
	void updatePower(Level& level, const BlockPos& pos) const;
	bool findPoweredRailSignal(Level& level, const BlockPos& pos, int block, bool forward, int distance) const;
};

#endif
