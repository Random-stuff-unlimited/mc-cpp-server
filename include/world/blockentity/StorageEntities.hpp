#ifndef STORAGE_ENTITIES_HPP
#define STORAGE_ENTITIES_HPP

#include "world/blockentity/ContainerEntities.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

// The block entities of the other blocks holding items: chiseled bookshelves, decorated pots, jukeboxes, lecterns,
// crafters and shelves. Their blocks' behaviors are in world/blocks/StorageBlocks.hpp

// Its block entity if the type is one of these, nullptr otherwise
std::unique_ptr<BlockEntity> createStorageBlockEntity(const std::string& type, const BlockPos& pos);

// ServerLevel.sendParticles of a particle type without options
void sendParticles(Level& level, const char* particle, double x, double y, double z, int count, double dx, double dy, double dz, double speed);
// Containers.dropContents: every stack in pieces at pos
void dropContents(Level& level, const BlockPos& pos, Container& container);

// Network NBT of the update tags (what the client draws)
namespace UpdateTag {
	// Items: a list of {Slot, id, count} for the non-empty stacks (ContainerHelper.saveAllItems). Components aren't
	// written: the client draws the item without them
	void items(std::vector<uint8_t>& out, const std::vector<ItemStack>& stacks, const GameData& gameData);
} // namespace UpdateTag

// ChiseledBookShelfBlockEntity: 6 books (one each). The block's slot_N_occupied properties follow the slots; comparators
// read the last slot used (1 to 6)
class ChiseledBookShelfBlockEntity : public BlockEntity, public Container {
  public:
	explicit ChiseledBookShelfBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:chiseled_bookshelf", pos), _items(6) {}

	int		   size() const override { return 6; }
	ItemStack& item(int slot) override { return _items.at(slot); }
	void	   setItem(int slot, ItemStack stack) override;
	ItemStack  removeItem(int slot, int count) override;
	int		   maxStackSize() const override { return 1; }
	bool	   canPlaceItem(int slot, const ItemStack& stack) const override;
	bool	   canTakeItem(Container& target, int slot, const ItemStack& stack) override;
	void	   setChanged() override { markChanged(); }
	bool	   stillValid(Player& player) override;
	void	   preRemoveSideEffects(Level& level) override { dropContents(level, pos(), *this); }
	void	   save(BlockEntityWriter& out) const override;
	void	   load(BlockEntityReader& in) override;

	// acceptsItemType: #minecraft:bookshelf_books
	bool acceptsItemType(const ItemStack& stack) const;
	int	 lastInteractedSlot() const { return _lastInteractedSlot; }
	std::vector<ItemStack>&		  items() { return _items; }
	const std::vector<ItemStack>& items() const { return _items; }

  private:
	std::vector<ItemStack> _items;
	int					   _lastInteractedSlot = -1;
	// updateState: the slot was used, the block shows which slots hold a book
	void				   updateState(int slot);
};

// DecoratedPotBlockEntity: one stack of items (ContainerSingleItem) and the sherds of its 4 sides
class DecoratedPotBlockEntity : public BlockEntity, public Container {
  public:
	// WobbleStyle: its ordinal is the block event's data
	enum class Wobble { Positive = 0, Negative = 1 };
	explicit DecoratedPotBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:decorated_pot", pos) {}

	int		   size() const override { return 1; }
	ItemStack& item(int slot) override { return slot == 0 ? _item : _none; }
	void	   setItem(int slot, ItemStack stack) override;
	ItemStack  removeItem(int slot, int count) override;
	void	   setChanged() override { markChanged(); }
	bool	   stillValid(Player& player) override;
	void	   preRemoveSideEffects(Level& level) override { dropContents(level, pos(), *this); }
	void	   save(BlockEntityWriter& out) const override;
	void	   load(BlockEntityReader& in) override;
	void	   writeUpdateTag(std::vector<uint8_t>& out) const override;
	bool	   hasUpdatePacket() const override { return true; }

	// PotDecorations: back, left, right, front item ids, 0 for none (a brick side)
	std::array<int, 4> decorations{0, 0, 0, 0};
	ItemStack&		   theItem() { return _item; }
	const ItemStack&   theItem() const { return _item; }
	// PotDecorations.ordered: the 4 sides' items, bricks where there is none
	std::array<int, 4> orderedDecorations(const GameData& gameData) const;
	// wobble: a block event the clients animate
	void			   wobble(Wobble style);

  private:
	ItemStack		_item;
	ItemStack		_none;
	const GameData* _gameData = nullptr; // For the update tag, which may be written before it is in the level
};

// JukeboxSong: a song of the minecraft:jukebox_song registry (JukeboxSongs.bootstrap)
struct JukeboxSong {
	const char* name;			 // "minecraft:cat"
	float		lengthInSeconds;
	int			comparatorOutput;
	int			lengthInTicks() const;
	// hasFinished: 20 ticks after its end
	bool		hasFinished(int64_t ticks) const { return ticks >= lengthInTicks() + 20; }
	// fromStack: the song of the item's minecraft:jukebox_playable (the music discs'), nullptr if none
	static const JukeboxSong* fromStack(const ItemStack& stack, const GameData& gameData);
	static const JukeboxSong* byName(const std::string& name);
};

// JukeboxBlockEntity: one disc, and the song it plays (JukeboxSongPlayer)
class JukeboxBlockEntity : public BlockEntity, public Container {
  public:
	explicit JukeboxBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:jukebox", pos) {}

	int		   size() const override { return 1; }
	ItemStack& item(int slot) override { return slot == 0 ? _item : _none; }
	void	   setItem(int slot, ItemStack stack) override;
	ItemStack  removeItem(int slot, int count) override;
	int		   maxStackSize() const override { return 1; }
	bool	   canPlaceItem(int slot, const ItemStack& stack) const override;
	bool	   canTakeItem(Container& target, int slot, const ItemStack& stack) override;
	void	   setChanged() override { markChanged(); }
	bool	   stillValid(Player& player) override;
	bool	   ticks() const override { return true; }
	void	   tick(Level& level) override;
	// popOutTheItem, then setRemoved's stop event
	void	   preRemoveSideEffects(Level& level) override;
	void	   save(BlockEntityWriter& out) const override;
	void	   load(BlockEntityReader& in) override;

	const ItemStack& theItem() const { return _item; }
	// setTheItem: the block's has_record follows, the disc's song starts (or stops)
	void			 setTheItem(ItemStack stack);
	// popOutTheItem: the disc flies out of the top
	void			 popOutTheItem();
	bool			 isPlaying() const { return _song != nullptr; }
	const JukeboxSong* song() const { return _song; }
	int64_t			 ticksSinceSongStarted() const { return _ticksSinceSongStarted; }
	int				 comparatorOutput() const;

  private:
	ItemStack		   _item;
	ItemStack		   _none;
	const JukeboxSong* _song				  = nullptr;
	int64_t			   _ticksSinceSongStarted = 0;
	// JukeboxSongPlayer
	void			   play(const JukeboxSong& song);
	void			   stop();
	void			   onSongChanged();
};

// LecternBlockEntity: a book (writable or written) open at a page. Not a container: its menu reaches the book
// through bookAccess
class LecternBlockEntity : public BlockEntity {
  public:
	explicit LecternBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:lectern", pos) {}

	// bookAccess: the menu's one slot. Taking the book resets the block
	class BookAccess : public Container {
	  public:
		explicit BookAccess(LecternBlockEntity& lectern) : _lectern(lectern) {}
		int		   size() const override { return 1; }
		ItemStack& item(int slot) override { return slot == 0 ? _lectern._book : _none; }
		ItemStack  removeItem(int slot, int count) override;
		ItemStack  takeBook();
		void	   setItem(int, ItemStack) override {}
		int		   maxStackSize() const override { return 1; }
		void	   setChanged() override { _lectern.markChanged(); }
		bool	   stillValid(Player& player) override;
		bool	   canPlaceItem(int, const ItemStack&) const override { return false; }

	  private:
		LecternBlockEntity& _lectern;
		ItemStack			_none;
	};

	void save(BlockEntityWriter& out) const override;
	void load(BlockEntityReader& in) override;
	// Drops the book in front of it
	void preRemoveSideEffectsWithState(Level& level, int oldState) override;

	const ItemStack& book() const { return _book; }
	// hasBook: a writable or written book's content
	bool			 hasBook() const;
	void			 setBook(ItemStack book);
	int				 page() const { return _page; }
	// setPage: within the book; a change gives a redstone pulse
	void			 setPage(int page);
	// getRedstoneSignal: how far into the book it is open
	int				 redstoneSignal() const;
	BookAccess&		 bookAccess() { return _access; }

  private:
	ItemStack		_book;
	int				_page = 0, _pageCount = 0;
	BookAccess		_access{*this};
	const GameData* _gameData = nullptr; // Until it is in the level
	const GameData* data() const;
	void	   onBookItemRemove();
	int		   pageCount(const ItemStack& book) const;
};

// CrafterBlockEntity: a 3x3 crafting grid whose slots can be disabled, crafting once per redstone pulse
class CrafterBlockEntity : public ContainerBlockEntity {
  public:
	explicit CrafterBlockEntity(const BlockPos& pos) : ContainerBlockEntity("minecraft:crafter", pos, 9) {}
	std::string defaultName() const override { return "container.crafter"; }

	void setItem(int slot, ItemStack stack) override;
	bool canPlaceItem(int slot, const ItemStack& stack) const override;
	bool ticks() const override { return true; }
	void tick(Level& level) override; // serverTick

	// containerData: 0-8 the slot states (1 = disabled), 9 triggered
	int	 data(int index) const { return index == 9 ? _triggered : _slotStates.at(index); }
	void setSlotState(int slot, bool enabled);
	bool isSlotDisabled(int slot) const { return slot >= 0 && slot < 9 && _slotStates[slot] == 1; }
	void setTriggered(bool triggered) { _triggered = triggered ? 1 : 0; }
	bool isTriggered() const { return _triggered == 1; }
	void setCraftingTicksRemaining(int ticks) { _craftingTicksRemaining = ticks; }
	// getRedstoneSignal: the slots holding something or disabled
	int	 redstoneSignal();

  protected:
	void saveExtra(BlockEntityWriter& out) const override;
	void loadExtra(BlockEntityReader& in) override;

  private:
	std::array<int, 9> _slotStates{};
	int				   _triggered			   = 0;
	int				   _craftingTicksRemaining = 0;
	bool			   slotCanBeDisabled(int slot) const { return slot > -1 && slot < 9 && _items[slot].isEmpty(); }
	bool			   smallerStackExist(int count, const ItemStack& stack, int slot) const;
};

// ShelfBlockEntity: 3 items shown on the shelf, swapped with the hand or the hotbar
class ShelfBlockEntity : public BlockEntity, public Container {
  public:
	explicit ShelfBlockEntity(const BlockPos& pos) : BlockEntity("minecraft:shelf", pos), _items(3) {}

	int		   size() const override { return 3; }
	ItemStack& item(int slot) override { return _items.at(slot); }
	void	   setItem(int slot, ItemStack stack) override;
	// setChanged: saved, and the clients are sent its items
	void	   setChanged() override;
	bool	   stillValid(Player& player) override;
	void	   preRemoveSideEffects(Level& level) override { dropContents(level, pos(), *this); }
	void	   save(BlockEntityWriter& out) const override;
	void	   load(BlockEntityReader& in) override;
	void	   writeUpdateTag(std::vector<uint8_t>& out) const override;
	bool	   hasUpdatePacket() const override { return true; }

	// swapItemNoUpdate: puts the stack in the slot, returns what was there
	ItemStack					  swapItemNoUpdate(int slot, ItemStack stack);
	std::vector<ItemStack>&		  items() { return _items; }
	const std::vector<ItemStack>& items() const { return _items; }
	bool						  alignItemsToBottom = false;

  private:
	std::vector<ItemStack> _items;
	const GameData*		   _gameData = nullptr; // For the update tag, which may be written before it is in the level
};

#endif
