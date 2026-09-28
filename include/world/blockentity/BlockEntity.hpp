#ifndef BLOCK_ENTITY_HPP
#define BLOCK_ENTITY_HPP

#include "world/BlockPos.hpp"
#include "world/item/ItemStack.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GameData;
class Level;
class Player;

// Bytes a block entity is saved as (in the chunk). Little helpers to write and read them
class BlockEntityWriter {
  public:
	explicit BlockEntityWriter(const GameData& gameData) : gameData(gameData) {}
	const GameData&		 gameData;
	std::vector<uint8_t> out;

	void u8(uint8_t value) { out.push_back(value); }
	void varint(uint32_t value);
	void f32(float value);
	void string(const std::string& value);
	void bytes(const std::vector<uint8_t>& value);
	// Item by name (ids change between versions), count and component patch
	void item(const ItemStack& stack);
};

class BlockEntityReader {
  public:
	BlockEntityReader(const GameData& gameData, const uint8_t* data, size_t size) : gameData(gameData), _data(data), _size(size) {}
	const GameData& gameData;

	uint8_t				 u8();
	uint32_t			 varint();
	float				 f32();
	std::string			 string();
	std::vector<uint8_t> bytes();
	ItemStack			 item();
	// Everything left
	std::vector<uint8_t> rest() {
		std::vector<uint8_t> left(_data + _pos, _data + _size);
		_pos = _size;
		return left;
	}

  private:
	const uint8_t* _data;
	size_t		   _size;
	size_t		   _pos = 0;
	void		   need(size_t count) const;
};

// Vanilla's BlockEntity: data that goes with a block (comparator output, container items, moving piston...), kept
// in its chunk and saved with it. Game thread only; the chunk's mutex is held while it is saved
class BlockEntity {
  public:
	BlockEntity(std::string type, const BlockPos& pos) : _type(std::move(type)), _pos(pos) {}
	virtual ~BlockEntity() = default;

	const std::string& type() const { return _type; } // "minecraft:dispenser"...
	const BlockPos&	   pos() const { return _pos; }
	bool			   isRemoved() const { return _removed; }
	void			   setRemoved() { _removed = true; }
	// Set by the level when the block entity is in it
	Level*			   level() const { return _level; }
	void			   setLevel(Level* level) { _level = level; }
	// BlockEntity.setChanged: the chunk must be saved, comparators around read it again
	void			   markChanged();

	// Ticked in the block entity phase (only moving pistons for now)
	virtual bool ticks() const { return false; }
	virtual void tick(Level&) {}
	// The block is going away (BlockEntity.preRemoveSideEffects): containers drop their items...
	virtual void preRemoveSideEffects(Level&) {}

	virtual void save(BlockEntityWriter& out) const	  = 0;
	virtual void load(BlockEntityReader& in)		  = 0;
	// BlockEntity.getUpdateTag as network NBT (what the client renders: campfire items...). Nothing by default: the
	// client gets a null tag
	virtual void writeUpdateTag(std::vector<uint8_t>& out) const { out.push_back(0); }

	// Creates an empty block entity of a type (a generic one, that only keeps its data, for the types not ported)
	static std::unique_ptr<BlockEntity> create(const std::string& type, const BlockPos& pos);

  private:
	std::string _type;
	BlockPos	_pos;
	bool		_removed = false;
	Level*		_level	 = nullptr;
};

// A block entity of a type the server doesn't simulate (beds, signs, banners...): the client still needs it to
// draw the block, and its saved data is kept as is
class GenericBlockEntity : public BlockEntity {
  public:
	using BlockEntity::BlockEntity;
	void save(BlockEntityWriter& out) const override { out.out.insert(out.out.end(), _data.begin(), _data.end()); }
	void load(BlockEntityReader& in) override { _data = in.rest(); }

  private:
	std::vector<uint8_t> _data;
};

// ComparatorBlockEntity: the comparator's output
class ComparatorBlockEntity : public BlockEntity {
  public:
	explicit ComparatorBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:comparator", pos) {}
	int	 output = 0;
	void save(BlockEntityWriter& out) const override { out.varint(static_cast<uint32_t>(output)); }
	void load(BlockEntityReader& in) override { output = static_cast<int>(in.varint()); }
};

// PistonMovingBlockEntity: a block on its way. Its tick is set by the pistons' behavior
class PistonMovingBlockEntity : public BlockEntity {
  public:
	explicit PistonMovingBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:piston", pos) {}
	PistonMovingBlockEntity(const BlockPos& pos, int movedState, Direction direction, bool extending, bool sourcePiston)
		: BlockEntity("minecraft:piston", pos), movedState(movedState), direction(direction), extending(extending), sourcePiston(sourcePiston) {}

	int		  movedState   = 0;
	Direction direction	   = Direction::Down;
	bool	  extending	   = false;
	bool	  sourcePiston = false;
	float	  progress = 0.0F, progressO = 0.0F;
	int64_t	  lastTicked   = 0;

	bool ticks() const override { return true; }
	void tick(Level& level) override;
	// The move ends now (PistonMovingBlockEntity.finalTick)
	void finalTick(Level& level);
	void preRemoveSideEffects(Level& level) override { finalTick(level); }
	float extendedProgress(float value) const { return extending ? value - 1.0F : 1.0F - value; }
	Direction movementDirection() const { return extending ? direction : Directions::opposite(direction); }
	void save(BlockEntityWriter& out) const override;
	void load(BlockEntityReader& in) override;
};

// Vanilla's Container: numbered item slots. The "worldly" ones (WorldlyContainer: furnaces, brewing stands, shulker
// boxes) only show some slots on each side to hoppers
class Container {
  public:
	virtual ~Container() = default;
	virtual int		   size() const		   = 0;
	virtual ItemStack& item(int slot)	   = 0;
	virtual void	   setItem(int slot, ItemStack stack);
	// Container.getMaxStackSize: 99
	virtual int		   maxStackSize() const { return 99; }
	virtual bool	   canPlaceItem(int, const ItemStack&) const { return true; }
	// canTakeItem: whether `target` may take the item from this slot
	virtual bool	   canTakeItem(Container&, int, const ItemStack&) { return true; }
	virtual void	   setChanged() {}
	virtual bool	   stillValid(Player&) { return true; }
	virtual void	   startOpen(Player&) {}
	virtual void	   stopOpen(Player&) {}
	// What the slots really are: two views of the same inventory share it (AbstractContainerMenu.transferState)
	virtual const void* storage() const { return this; }
	// This container, or a part of it (a double chest's halves)
	virtual bool	   contains(const Container* other) const { return other == this; }

	// WorldlyContainer
	virtual bool			 isWorldly() const { return false; }
	virtual std::vector<int> slotsForFace(Direction face);
	// face: null for an item not going through a side (an item entity sucked by a hopper)
	virtual bool			 canPlaceItemThroughFace(int, const ItemStack&, const Direction*) { return true; }
	virtual bool			 canTakeItemThroughFace(int, const ItemStack&, Direction) { return true; }

	bool	  isEmpty();
	// Container.removeItem: takes up to count from the slot
	virtual ItemStack removeItem(int slot, int count);
	ItemStack removeItemNoUpdate(int slot);
	// getMaxStackSize(stack): the container's limit and the item's
	int		  maxStackSizeFor(const ItemStack& stack, const GameData& gameData) const;
};

#endif
