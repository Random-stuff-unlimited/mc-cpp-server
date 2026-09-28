#ifndef CONTAINER_ENTITIES_HPP
#define CONTAINER_ENTITIES_HPP

#include "world/blockentity/BlockEntity.hpp"

#include <memory>
#include <string>
#include <vector>

class JavaRandom;

// The block entities holding items (vanilla's BaseContainerBlockEntity and subclasses). Their blocks' behaviors are
// in world/blocks/Containers.hpp

// Its block entity if the type holds items, nullptr otherwise
std::unique_ptr<BlockEntity> createContainerBlockEntity(const std::string& type, const BlockPos& pos);
// The same for the block entities that process items (furnaces, brewing stands, crafters...)
std::unique_ptr<BlockEntity> createProcessingBlockEntity(const std::string& type, const BlockPos& pos);

// DoubleBlockCombiner for chests: the other half of a double chest at pos (nullptr if single, or if a half is
// blocked by a solid block above and ignoreBlocked is false; partnerBlocked then tells the other half is, which
// makes the whole chest unusable); first: whether pos is the first half (the right one)
std::shared_ptr<BlockEntity> chestPartner(Level& level, const BlockPos& pos, bool ignoreBlocked, bool& first, bool* partnerBlocked = nullptr);
// ChestBlock.isChestBlockedAt: a solid (redstone conductor) block above (cats don't exist yet)
bool						 isChestBlocked(Level& level, const BlockPos& pos);

// BaseContainerBlockEntity: slots, a custom name, items dropped when the block goes
class ContainerBlockEntity : public BlockEntity, public Container {
  public:
	ContainerBlockEntity(std::string type, const BlockPos& pos, int size) : BlockEntity(std::move(type), pos), _items(static_cast<size_t>(size)) {}

	int		   size() const override { return static_cast<int>(_items.size()); }
	ItemStack& item(int slot) override { return _items.at(slot); }
	void	   setItem(int slot, ItemStack stack) override;
	void	   setChanged() override { markChanged(); }
	// Container.stillValidBlockEntity: still there, and the player within reach
	bool	   stillValid(Player& player) override;
	// Containers.dropContents
	void	   preRemoveSideEffects(Level& level) override;
	void	   save(BlockEntityWriter& out) const override;
	void	   load(BlockEntityReader& in) override;

	// The name shown on its screen: the custom one (custom_name component, network NBT) or the default's key
	std::vector<uint8_t>		customName;
	virtual std::string			defaultName() const = 0; // "container.chest"
	std::vector<ItemStack>&		items() { return _items; }
	const std::vector<ItemStack>& items() const { return _items; }

  protected:
	std::vector<ItemStack> _items;
	// Extra data of a subclass, saved after the items
	virtual void saveExtra(BlockEntityWriter&) const {}
	virtual void loadExtra(BlockEntityReader&) {}
};

// Containers.dropItemStack: a stack in pieces of 10 to 30 items, at a random point of the block
void dropItemStack(Level& level, double x, double y, double z, ItemStack stack);

// ContainerOpenersCounter: how many players have it open (chest lids, barrel state, trapped chest power). Checked
// again every 5 ticks while open
class OpenersListener {
  public:
	virtual ~OpenersListener()										  = default;
	virtual void onOpen(Level& level)								  = 0;
	virtual void onClose(Level& level)								  = 0;
	virtual void openerCountChanged(Level& level, int before, int now) = 0;
	virtual bool isOwnContainer(Player& player)						  = 0;
};

class OpenersCounter {
  public:
	// range: the player's container interaction range (its block interaction range)
	void increment(Level& level, const BlockPos& pos, OpenersListener& listener, double range);
	void decrement(Level& level, const BlockPos& pos, OpenersListener& listener);
	void recheck(Level& level, const BlockPos& pos, OpenersListener& listener);
	int	 count() const { return _count; }
	// getEntitiesWithContainerOpen: the players around that have it open
	int	 playersWithContainerOpen(Level& level, const BlockPos& pos, OpenersListener& listener, double* maxRange = nullptr) const;

  private:
	int	   _count				= 0;
	double _maxInteractionRange = 0.0;
};
// Player.blockInteractionRange (the attribute's base value)
double blockInteractionRange(const Player& player);

// CompoundContainer: a double chest's two halves as one (the first one's slots, then the second's)
class CompoundContainer : public Container {
  public:
	CompoundContainer(std::shared_ptr<BlockEntity> first, std::shared_ptr<BlockEntity> second);
	int		   size() const override { return _first->size() + _second->size(); }
	ItemStack& item(int slot) override { return slot < _first->size() ? _first->item(slot) : _second->item(slot - _first->size()); }
	void	   setItem(int slot, ItemStack stack) override;
	int		   maxStackSize() const override { return _first->maxStackSize(); }
	void	   setChanged() override;
	bool	   stillValid(Player& player) override { return _first->stillValid(player) && _second->stillValid(player); }
	void	   startOpen(Player& player) override;
	void	   stopOpen(Player& player) override;
	bool	   contains(const Container* other) const override { return other == _first || other == _second; }

  private:
	std::shared_ptr<BlockEntity> _keepFirst, _keepSecond;
	Container *					 _first, *_second;
};

// ChestBlockEntity (chests, trapped chests, copper chests): 27 slots, a lid, a trapped chest's power
class ChestBlockEntity : public ContainerBlockEntity, public OpenersListener {
  public:
	ChestBlockEntity(std::string type, const BlockPos& pos) : ContainerBlockEntity(std::move(type), pos, 27) {}
	std::string defaultName() const override { return "container.chest"; }
	void		startOpen(Player& player) override;
	void		stopOpen(Player& player) override;
	void		recheckOpen() {
		   if (level()) _openers.recheck(*level(), pos(), *this);
	}
	int			openCount() const { return _openers.count(); }
	// getEntitiesWithContainerOpen: how many players around have it open
	int			playersWithContainerOpen() { return level() ? _openers.playersWithContainerOpen(*level(), pos(), *this) : 0; }

	void onOpen(Level& level) override;
	void onClose(Level& level) override;
	void openerCountChanged(Level& level, int before, int now) override;
	bool isOwnContainer(Player& player) override;

  private:
	OpenersCounter _openers;
	void		   playSound(Level& level, bool open);
};

// BarrelBlockEntity: 27 slots, the block's open state
class BarrelBlockEntity : public ContainerBlockEntity, public OpenersListener {
  public:
	explicit BarrelBlockEntity(const BlockPos& pos) : ContainerBlockEntity("minecraft:barrel", pos, 27) {}
	std::string defaultName() const override { return "container.barrel"; }
	void		startOpen(Player& player) override;
	void		stopOpen(Player& player) override;
	void		recheckOpen() {
		   if (level()) _openers.recheck(*level(), pos(), *this);
	}
	void onOpen(Level& level) override;
	void onClose(Level& level) override;
	void openerCountChanged(Level&, int, int) override {}
	bool isOwnContainer(Player& player) override;

  private:
	OpenersCounter _openers;
	void		   setOpen(Level& level, bool open);
};

// ShulkerBoxBlockEntity: 27 slots that stay with the box when it is broken; no shulker box inside another. Its lid
// opens over 10 ticks, pushing entities out of the way
class ShulkerBoxBlockEntity : public ContainerBlockEntity {
  public:
	enum class Animation { Closed, Opening, Opened, Closing };
	explicit ShulkerBoxBlockEntity(const BlockPos& pos) : ContainerBlockEntity("minecraft:shulker_box", pos, 27) {}
	std::string defaultName() const override { return "container.shulkerBox"; }
	void		startOpen(Player& player) override;
	void		stopOpen(Player& player) override;
	void		preRemoveSideEffects(Level&) override {} // Its items go with the dropped box
	bool		ticks() const override { return true; }
	void		tick(Level& level) override;
	// The open count block event (type 1)
	bool		triggerEvent(int type, int data);
	Animation	animation() const { return _animation; }
	float		progress() const { return _progress; }

	bool			 isWorldly() const override { return true; }
	bool			 canPlaceItemThroughFace(int, const ItemStack& stack, const Direction*) override;
	bool			 canPlaceItem(int, const ItemStack& stack) const override { return !isShulkerBoxItem(stack); }
	bool			 isShulkerBoxItem(const ItemStack& stack) const;

  private:
	int		  _openCount = 0;
	Animation _animation = Animation::Closed;
	float	  _progress = 0.0F, _progressOld = 0.0F;
};

// EnderChestBlockEntity: only a lid; the items are the player's
class EnderChestBlockEntity : public BlockEntity, public OpenersListener {
  public:
	explicit EnderChestBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:ender_chest", pos) {}
	void startOpen(Player& player);
	void stopOpen(Player& player);
	void recheckOpen() {
		if (level()) _openers.recheck(*level(), pos(), *this);
	}
	bool stillValid(Player& player);
	void save(BlockEntityWriter&) const override {}
	void load(BlockEntityReader&) override {}

	void onOpen(Level& level) override;
	void onClose(Level& level) override;
	void openerCountChanged(Level& level, int before, int now) override;
	bool isOwnContainer(Player& player) override;

  private:
	OpenersCounter _openers;
};

// PlayerEnderChestContainer: the player's 27 ender chest slots, while an ender chest shows them
class EnderChestContainer : public Container {
  public:
	EnderChestContainer(Player& player, std::shared_ptr<BlockEntity> chest);
	int		   size() const override { return 27; }
	ItemStack& item(int slot) override;
	bool	   stillValid(Player& player) override { return _chest->stillValid(player); }
	void	   startOpen(Player& player) override { _chest->startOpen(player); }
	void	   stopOpen(Player& player) override { _chest->stopOpen(player); }
	EnderChestBlockEntity* chest() const { return _chest; }

  private:
	Player&						 _player;
	std::shared_ptr<BlockEntity> _keepAlive;
	EnderChestBlockEntity*		 _chest;
};

// HopperBlockEntity: 5 slots; every 8 ticks pushes one item forward and pulls one from above
class HopperBlockEntity : public ContainerBlockEntity {
  public:
	explicit HopperBlockEntity(const BlockPos& pos) : ContainerBlockEntity("minecraft:hopper", pos, 5) {}
	std::string defaultName() const override { return "container.hopper"; }
	bool		ticks() const override { return true; }
	void		tick(Level& level) override; // pushItemsTick
	// entityInside: an item entity in the hopper's suck area
	void		entityInside(Level& level, class ItemEntity& item);

	int		cooldown	 = -1;
	int64_t tickedGameTime = 0;

  private:
	bool tryMoveItems(Level& level, class ItemEntity* only);
	bool inventoryFull(Level& level);
	bool ejectItems(Level& level);
	bool suckInItems(Level& level);
	bool suckItem(Level& level, class ItemEntity& item);

  protected:
	void saveExtra(BlockEntityWriter& out) const override { out.varint(static_cast<uint32_t>(cooldown + 1)); }
	void loadExtra(BlockEntityReader& in) override { cooldown = static_cast<int>(in.varint()) - 1; }
};

// DispenserBlockEntity and DropperBlockEntity: 9 slots
class DispenserBlockEntity : public ContainerBlockEntity {
  public:
	DispenserBlockEntity(const BlockPos& pos, bool dropper) : ContainerBlockEntity(dropper ? "minecraft:dropper" : "minecraft:dispenser", pos, 9) {}
	std::string defaultName() const override { return isDropper() ? "container.dropper" : "container.dispenser"; }
	bool		isDropper() const { return type() == "minecraft:dropper"; }
	// getRandomSlot: one of the non-empty slots, each as likely (-1 if all empty)
	int			getRandomSlot(JavaRandom& random);
	// insertItem: into the first slot holding the same item or empty, then the next...; returns what didn't fit
	ItemStack	insertItem(ItemStack stack, const GameData& gameData);
};

// Hopper transfers (HopperBlockEntity.addItem and getContainerAt), for hoppers and droppers
namespace Hoppers {
	// The container at pos (a block's), nullptr if none. The shared pointer keeps it (a double chest) alive
	std::shared_ptr<Container> containerAt(Level& level, const BlockPos& pos);
	// Moves as much of the stack as fits into `to` (through `face`, null if not through a side); returns the rest
	ItemStack addItem(Level& level, Container* from, Container& to, ItemStack stack, const Direction* face);
} // namespace Hoppers

#endif
