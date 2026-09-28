#ifndef REDSTONE_HPP
#define REDSTONE_HPP

#include "world/BlockBehavior.hpp"
#include "world/blocks/Attached.hpp"
#include "world/blocks/BlockContext.hpp"

#include <memory>
#include <string>
#include <vector>

// Redstone components, ported from their vanilla classes. Directions given to getSignal go from the block asking
// to the block giving power (SignalGetter's convention)

// Property and value ids the components share
struct RedstoneIds {
	int power, powered, lit, face, delay, locked, mode, note, instrument, open, hinge, inWall;
	int sides[4]; // north, east, south, west (RedstoneSide properties of the wire)
	int none, side, up, floor, wall, ceiling, compare, subtract;
	int wire, repeater, comparator, observer, redstoneBlock, hopper, air;

	explicit RedstoneIds(const BlockContext& context);
	// Index in sides of a horizontal direction
	static int sideIndex(Direction direction);
};

// Direction.getClockWise / getCounterClockWise, on the horizontal plane
Direction clockWise(Direction direction);
Direction counterClockWise(Direction direction);

class RedstoneBehavior : public BlockBehavior {
  public:
	RedstoneBehavior(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids) : _context(std::move(context)), _ids(std::move(ids)) {}

  protected:
	std::shared_ptr<const BlockContext> _context;
	std::shared_ptr<const RedstoneIds>	_ids;
	const BlockRegistry&				blocks() const { return _context->blocks; }
	int									blockOf(int state) const { return _context->blocks.blockOf(state); }
};

// RedStoneWireBlock with the DefaultRedstoneWireEvaluator (the one vanilla uses without the experiment)
class RedStoneWireBlock : public RedstoneBehavior {
  public:
	RedStoneWireBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids);

	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void updateIndirectNeighbourShapes(Level& level, const BlockPos& pos, int state, int flags, int limit) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool isSignalSource(int state) const override { return _shouldSignal; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;

  private:
	int			 _crossState;
	// Off while the wire measures the power around it, so it doesn't count itself (vanilla's shouldSignal)
	mutable bool _shouldSignal = true;

	bool connected(int state, Direction direction) const;
	bool isCross(int state) const;
	bool isDot(int state) const;
	bool canSurviveOn(Level& level, const BlockPos& pos, int state) const;
	bool shouldConnectTo(Level& level, int state, const Direction* direction) const;
	int	 getConnectingSide(Level& level, const BlockPos& pos, Direction direction) const;
	int	 getConnectingSide(Level& level, const BlockPos& pos, Direction direction, bool nonConductorAbove) const;
	int	 getMissingConnections(Level& level, int state, const BlockPos& pos) const;
	int	 getConnectionState(Level& level, int state, const BlockPos& pos) const;
	void updatePowerStrength(Level& level, const BlockPos& pos, int state) const;
	int	 wireSignal(int state) const;
	void checkCornerChangeAt(Level& level, const BlockPos& pos) const;
	void updateNeighborsOfNeighboringWires(Level& level, const BlockPos& pos) const;
};

// RedstoneTorchBlock and RedstoneWallTorchBlock: off when their support is powered, burn out after 8 toggles in 60
// ticks
class RedstoneTorchBlock : public RedstoneBehavior {
  public:
	RedstoneTorchBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool wall);

	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	bool _wall;
	int	 _floorTorch, _wallTorch;
	bool hasNeighborSignal(Level& level, const BlockPos& pos, int state) const;
	bool isToggledTooFrequently(Level& level, const BlockPos& pos, bool logToggle) const;
	void notifyNeighbors(Level& level, const BlockPos& pos, int state) const;
};

// DiodeBlock: repeaters and comparators
class DiodeBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;

	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void setPlacedBy(Level& level, const BlockPos& pos, int state) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  protected:
	Direction	 facing(int state) const { return _context->direction(state, _context->facing); }
	bool		 canSurviveOn(Level& level, int below) const;
	virtual bool isLocked(Level& level, const BlockPos& pos, int state) const;
	virtual bool shouldTurnOn(Level& level, const BlockPos& pos, int state) const;
	virtual int	 getInputSignal(Level& level, const BlockPos& pos, int state) const;
	int			 getAlternateSignal(Level& level, const BlockPos& pos, int state) const;
	virtual bool sideInputDiodesOnly() const { return false; }
	virtual int	 getOutputSignal(Level& level, const BlockPos& pos, int state) const;
	virtual int	 getDelay(int state) const = 0;
	virtual void checkTickOnNeighbor(Level& level, const BlockPos& pos, int state) const;
	void		 updateNeighborsInFront(Level& level, const BlockPos& pos, int state) const;
	bool		 shouldPrioritize(Level& level, const BlockPos& pos, int state) const;
};

class RepeaterBlock : public DiodeBlock {
  public:
	using DiodeBlock::DiodeBlock;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;

  protected:
	bool isLocked(Level& level, const BlockPos& pos, int state) const override;
	bool sideInputDiodesOnly() const override { return true; }
	int	 getDelay(int state) const override;
};

// ComparatorBlock: its output is kept by the level (ComparatorBlockEntity)
class ComparatorBlock : public DiodeBlock {
  public:
	using DiodeBlock::DiodeBlock;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;

  protected:
	bool shouldTurnOn(Level& level, const BlockPos& pos, int state) const override;
	int	 getInputSignal(Level& level, const BlockPos& pos, int state) const override;
	int	 getOutputSignal(Level& level, const BlockPos& pos, int state) const override;
	int	 getDelay(int) const override { return 2; }
	void checkTickOnNeighbor(Level& level, const BlockPos& pos, int state) const override;

  private:
	int	 calculateOutputSignal(Level& level, const BlockPos& pos, int state) const;
	void refreshOutputState(Level& level, const BlockPos& pos, int state) const;
};

// FaceAttachedHorizontalDirectionalBlock: levers and buttons, on a floor, a wall or a ceiling
class FaceAttachedBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  protected:
	// getConnectedDirection: from the block toward what holds it... reversed: the side it is attached on
	Direction connectedDirection(int state) const;
	void	  updateNeighbours(Level& level, const BlockPos& pos, int state) const;
};

class LeverBlock : public FaceAttachedBlock {
  public:
	using FaceAttachedBlock::FaceAttachedBlock;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
};

class ButtonBlock : public FaceAttachedBlock {
  public:
	// ticksToStayPressed: 30 for wooden buttons, 20 for stone ones. sound: the block set's sound prefix
	ButtonBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, int ticksToStayPressed, std::string sound);
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;

  private:
	int			_ticksToStayPressed;
	std::string _sound;
};

// BasePressurePlateBlock: pressed by entities on it. Wooden plates: any entity, stone ones: players only (no mobs
// yet), weighted plates: power by the number of entities
class PressurePlateBlock : public RedstoneBehavior {
  public:
	enum class Kind { Everything, Mobs, Weighted };
	PressurePlateBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, Kind kind, int maxWeight, std::string sound);

	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void entityInside(Level& level, const BlockPos& pos, int state) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	Kind		_kind;
	int			_maxWeight;
	std::string _sound;

	int	 signalForState(int state) const;
	int	 withSignal(int state, int signal) const;
	int	 signalStrength(Level& level, const BlockPos& pos) const;
	void checkPressed(Level& level, const BlockPos& pos, int state, int signal) const;
	void updateNeighbours(Level& level, const BlockPos& pos) const;
};

// ObserverBlock: a 2-tick pulse when the block it faces changes
class ObserverBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	void affectNeighborsAfterRemoval(Level& level, const BlockPos& pos, int state, bool movedByPiston) const override;
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
	int	 getDirectSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	void updateNeighborsInFront(Level& level, const BlockPos& pos, int state) const;
};

// RedstoneLampBlock: on at once, off 4 ticks after losing power
class RedstoneLampBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	void tick(Level& level, const BlockPos& pos, int state) const override;
};

// PoweredBlock (block of redstone): 15 everywhere
class PoweredBlock : public BlockBehavior {
  public:
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level&, const BlockPos&, int, Direction) const override { return 15; }
};

// TargetBlock: powered by projectiles (none yet); it still gives its power and resets
class TargetBlock : public RedstoneBehavior {
  public:
	using RedstoneBehavior::RedstoneBehavior;
	void tick(Level& level, const BlockPos& pos, int state) const override;
	void onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool movedByPiston) const override;
	bool isSignalSource(int) const override { return true; }
	int	 getSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;
};

// Doors, trapdoors and fence gates: open with power, or by hand (not the iron ones)
class PoweredDoorBlock : public DoorBlock {
  public:
	PoweredDoorBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool openByHand, std::string sound);
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;
	bool canSurvive(Level& level, const BlockPos& pos, int state) const override;

  private:
	std::shared_ptr<const RedstoneIds> _ids;
	bool							   _openByHand;
	std::string						   _sound;
};

class TrapDoorBlock : public RedstoneBehavior {
  public:
	TrapDoorBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, bool openByHand, std::string sound);
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;

  private:
	bool		_openByHand;
	std::string _sound;
};

class FenceGateBlock : public RedstoneBehavior {
  public:
	FenceGateBlock(std::shared_ptr<const BlockContext> context, std::shared_ptr<const RedstoneIds> ids, std::string sound);
	int	 getStateForPlacement(Level& level, const PlaceContext& context) const override;
	int	 updateShape(Level& level, const BlockPos& pos, int state, Direction direction, const BlockPos& neighborPos, int neighborState) const override;
	void neighborChanged(Level& level, const BlockPos& pos, int state, int sourceBlock, bool movedByPiston) const override;
	bool useWithoutItem(Level& level, const BlockPos& pos, int state, Player& player) const override;

  private:
	std::string		  _sound;
	std::vector<bool> _walls;
	bool			  isWall(int state) const { return _context->inTag(_walls, state); }
};

// Blocks comparators read from their state alone (cake, composter, cauldrons, end portal frame, respawn anchor, beehive)
class AnalogOutputBlock : public BlockBehavior {
  public:
	enum class Kind { Cake, CandleCake, Level, LavaCauldron, EndPortalFrame, RespawnAnchor, HoneyLevel };
	AnalogOutputBlock(std::shared_ptr<const BlockContext> context, Kind kind);
	int getAnalogOutputSignal(Level& level, const BlockPos& pos, int state, Direction direction) const override;

  private:
	std::shared_ptr<const BlockContext> _context;
	Kind								_kind;
	int									_property;
};

#endif
