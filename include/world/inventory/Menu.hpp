#ifndef MENU_HPP
#define MENU_HPP

#include "data/GameData.hpp"
#include "world/blockentity/BlockEntity.hpp"
#include "world/blockentity/ContainerEntities.hpp"
#include "world/item/ItemStack.hpp"
#include "world/item/Recipes.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Buffer;
class Level;
class Player;
class PlayerInventory;

// What the client says a slot holds after its click (HashedStack): the item and count, and whether its components
// differ from the item's defaults. Components are only compared as "none": a stack with components never matches,
// so the server sends it again, which is harmless
struct HashedStack {
	bool empty		   = true;
	int	 item		   = 0;
	int	 count		   = 0;
	bool noComponents  = true;

	static HashedStack read(Buffer& buf);
	bool			   matches(const ItemStack& stack) const;
};

// Vanilla's ClickType
enum class ClickType { Pickup = 0, QuickMove = 1, Swap = 2, Clone = 3, Throw = 4, QuickCraft = 5, PickupAll = 6 };

// The player's inventory as vanilla's Inventory container: 0-8 hotbar, 9-35 main, 36-39 armor (feet to head), 40 offhand
class PlayerContainer : public Container {
  public:
	explicit PlayerContainer(PlayerInventory& inventory) : _inventory(inventory) {}
	int			size() const override { return 41; }
	ItemStack&	item(int slot) override;
	const void* storage() const override { return &_inventory; }

  private:
	PlayerInventory& _inventory;
};

// Window slots of the player's inventory seen as another container (the 2x2 crafting grid, its result)
class InventoryRange : public Container {
  public:
	InventoryRange(PlayerInventory& inventory, int first, int count) : _inventory(inventory), _first(first), _count(count) {}
	int		   size() const override { return _count; }
	ItemStack& item(int slot) override;
	void	   setChanged() override {
		  if (changed) changed();
	}
	std::function<void()> changed; // A menu showing it follows it (slotsChanged)

  private:
	PlayerInventory& _inventory;
	int				 _first, _count;
};

// Slots of their own, only while a menu is open (SimpleContainer: a crafting grid, its result)
class SimpleContainer : public Container {
  public:
	explicit SimpleContainer(int size) : _items(size) {}
	int		   size() const override { return static_cast<int>(_items.size()); }
	ItemStack& item(int slot) override { return _items.at(slot); }
	void	   setChanged() override {
		  if (changed) changed();
	}
	std::function<void()> changed; // A menu showing it follows it (slotsChanged)

  private:
	std::vector<ItemStack> _items;
};

// ResultContainer: one slot, taken whole
class ResultContainer : public SimpleContainer {
  public:
	ResultContainer() : SimpleContainer(1) {}
	ItemStack removeItem(int slot, int) override { return removeItemNoUpdate(slot); }
};

// Vanilla's Slot: a slot of a container, as the menu shows it
struct Slot {
	// NonInteractive: NonInteractiveResultSlot (the crafter's result, only shown); Crafter: CrafterSlot (nothing goes in
	// a disabled slot)
	enum class Kind { Normal, Armor, Result, NoShulkerBox, NonInteractive, Crafter };

	Container*				container;
	int						containerSlot;
	int						index = 0; // In the menu
	Kind					kind  = Kind::Normal;
	GameData::EquipmentSlot armor = GameData::EquipmentSlot::MainHand; // Armor slots: which one
	// ----- Furnaces and brewing stands -----
	// Rules of a slot class of its own (FurnaceFuelSlot, PotionSlot...): which items it takes (Slot.mayPlace), and its
	// stack limit for an item (Slot.getMaxStackSize(stack); 0: the default one)
	std::function<bool(const ItemStack&)> placeRule;
	std::function<int(const ItemStack&)>  stackLimit;
	// ----- End furnaces and brewing stands -----

	ItemStack& item() const { return container->item(containerSlot); }
	bool	   hasItem() const { return !item().isEmpty(); }
	bool	   mayPlace(const ItemStack& stack, const GameData& gameData) const;
	bool	   mayPickup() const { return kind != Kind::NonInteractive; }
	int		   maxStackSize() const { return kind == Kind::Armor ? 1 : container->maxStackSize(); }
	int		   maxStackSize(const ItemStack& stack, const GameData& gameData) const;
	void	   set(ItemStack stack) const;
	void	   setChanged() const { container->setChanged(); }
	ItemStack  remove(int count) const { return container->removeItem(containerSlot, count); }
	// Slot.tryRemove / safeTake / safeInsert
	std::optional<ItemStack> tryRemove(int count, int decrement, const GameData& gameData) const;
	ItemStack				 safeTake(int count, int decrement, const GameData& gameData) const;
	// Puts up to `count` of the stack into the slot; the stack keeps the rest
	void					 safeInsert(ItemStack& stack, int count, const GameData& gameData) const;
};

// Vanilla's AbstractContainerMenu: slots over containers, the item held by the cursor, and what the client was last
// told about each (to send only the changes)
class Menu {
  public:
	Menu(Level& level, Player& player, int containerId, std::string type);
	virtual ~Menu() = default;

	int				   containerId() const { return _containerId; }
	const std::string& type() const { return _type; } // minecraft:menu registry name, empty for the inventory
	std::vector<Slot>& slots() { return _slots; }
	ItemStack&		   carried() { return _carried; }
	int				   stateId() const { return _stateId; }
	int				   incrementStateId() { return _stateId = (_stateId + 1) & 32767; }

	virtual ItemStack quickMoveStack(int slot) = 0;
	virtual bool	  stillValid()			   = 0;
	// The menu closes: the cursor's item goes back to the inventory
	virtual void removed();

	// AbstractContainerMenu.clicked
	void clicked(int slot, int button, ClickType type);
	// clickMenuButton (Container Button Click packet): a lectern's page buttons...; true if it did something
	virtual bool clickMenuButton(int) { return false; }
	bool isValidSlotIndex(int slot) const { return slot == -1 || slot == -999 || slot < static_cast<int>(_slots.size()); }

	// Synchronization with the client (ContainerSynchronizer)
	void sendAllDataToRemote();
	void broadcastChanges();
	void broadcastFullState() { sendAllDataToRemote(); }
	void setRemoteSlot(int slot, const ItemStack& stack);
	void setRemoteSlotUnsafe(int slot, const HashedStack& stack);
	void setRemoteCarried(const HashedStack& stack);
	void suppressRemoteUpdates() { _suppressRemoteUpdates = true; }
	void resumeRemoteUpdates() { _suppressRemoteUpdates = false; }
	bool synchronized() const { return _synchronized; }
	// transferState: what the client knows of the slots both menus show (the player's inventory) carries over
	void transferState(Menu& from);

  protected:
	Level&			  _level;
	Player&			  _player;
	const GameData&	  _gameData;
	std::vector<Slot> _slots;

	Slot& addSlot(Container& container, int containerSlot, Slot::Kind kind = Slot::Kind::Normal);
	// addDataSlots: values the client's screen shows (a furnace's burn and cook progress...), read through dataSlot()
	// and sent when they change (ContainerData, CONTAINER_SET_DATA)
	void		addDataSlots(int count) { _remoteData.assign(static_cast<size_t>(count), 0); }
	virtual int dataSlot(int) const { return 0; }
	// Run first by broadcastChanges: a menu that follows its slots (ContainerListener.slotChanged) looks at them here
	virtual void beforeBroadcastChanges() {}
	// moveItemStackTo: into the slots [start, end), stacks of the same item first
	bool  moveItemStackTo(ItemStack& stack, int start, int end, bool reverse);
	int	  maxStackSize(const ItemStack& stack) const;
	void  dropOrPlaceInInventory(ItemStack stack);
	void  clearContainer(Container& container);
	// Slot.onTake: an item was taken from the slot (a crafting result uses its ingredients up)
	virtual void onTake(Slot& slot, const ItemStack& stack) { slot.setChanged(); }
	// Slot.safeTake, then onTake
	ItemStack	 safeTake(Slot& slot, int count, int decrement);
	void		 sendSlot(int slot, const ItemStack& stack);
	void		 sendData(int slot, int value);
	// CraftingMenu.slotChangedCraftingGrid: the result of what the grid (width wide) holds, sent right away (hint: the
	// recipe to try first)
	void		 updateCraftingResult(Container& grid, int width, Container& result, const Recipe* hint = nullptr);
	// ResultSlot.onTake: one of each ingredient used, what they leave behind (buckets...) put back
	void		 takeCraftingResult(Container& grid, int width);

  private:
	// What the client holds in a slot: a stack we sent, or the hash it told us
	struct RemoteSlot {
		std::optional<ItemStack>   stack;
		std::optional<HashedStack> hash;
		bool					   matches(const ItemStack& actual);
		void					   force(const ItemStack& actual) {
			  stack = actual;
			  hash.reset();
		}
	};

	int						 _containerId;
	std::string				 _type;
	ItemStack				 _carried;
	int						 _stateId = 0;
	std::vector<RemoteSlot>	 _remoteSlots;
	RemoteSlot				 _remoteCarried;
	bool					 _suppressRemoteUpdates = false;
	bool					 _synchronized			= false;
	std::vector<int>		 _remoteData; // The data slots' values the client has
	int						 _quickcraftType		= -1;
	int						 _quickcraftStatus		= 0;
	std::vector<int>		 _quickcraftSlots; // Menu slot indexes, in the order they were dragged over

	void doClick(int slot, int button, ClickType type);
	void resetQuickCraft() {
		_quickcraftStatus = 0;
		_quickcraftSlots.clear();
	}
	bool canItemQuickReplace(const Slot* slot, const ItemStack& stack, bool ignoreCount) const;
};

// AbstractCraftingMenu: a crafting grid (its menu slots from 1) and its result (menu slot 0) that follows it; the
// recipe book fills the grid (RecipeBookMenu)
class CraftingGridMenu : public Menu {
  public:
	// handlePlacement (ServerPlaceRecipe.placeRecipe): the recipe's ingredients from the inventory into the grid, as
	// many times as asked (useMaxItems: as many as possible). True when the book should show it as a ghost recipe
	// (not enough items)
	bool placeRecipe(const Recipe& recipe, bool useMaxItems);

  protected:
	CraftingGridMenu(Level& level, Player& player, int containerId, std::string type, int width, int height);
	// The derived menu's grid, once its slots are added
	void setGrid(Container& grid);
	void onTake(Slot& slot, const ItemStack& stack) override;

	Container*		_grid = nullptr;
	ResultContainer _result;
	int				_gridWidth, _gridHeight;
	bool			_placingRecipe = false; // The result waits for the whole recipe to be placed

  private:
	void clearGrid();
	bool testClearGrid();
	// moveItemToGrid: up to count of the item from the inventory into the slot; what is left, -1 if none is there
	int	 moveItemToGrid(Slot& slot, int item, int count);
};

// InventoryMenu: the player's own inventory (container 0): crafting grid and result, armor, inventory, offhand
class InventoryMenu : public CraftingGridMenu {
  public:
	InventoryMenu(Level& level, Player& player, PlayerInventory& inventory);
	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override { return true; }
	void	  removed() override;

  private:
	PlayerContainer _inventory;
	InventoryRange	_craft;
};

// A container's slots and the player's inventory (ChestMenu, HopperMenu, DispenserMenu, ShulkerBoxMenu): shift-click
// moves between the two
class ContainerMenu : public Menu {
  public:
	// type: the minecraft:menu entry ("minecraft:generic_9x3"...); kind: its slots' kind (a shulker box's refuse
	// shulker boxes)
	ContainerMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, std::string type, std::shared_ptr<Container> container,
				  Slot::Kind kind = Slot::Kind::Normal);
	ItemStack  quickMoveStack(int slot) override;
	bool	   stillValid() override { return _container->stillValid(_player); }
	void	   removed() override;
	Container& container() { return *_container; }

  private:
	PlayerContainer			   _inventory;
	std::shared_ptr<Container> _container; // Keeps it even if its block is broken while open
};

// CraftingMenu: a crafting table's 3x3 grid and its result, then the player's inventory. The grid goes back to the
// player when it closes
class CraftingMenu : public CraftingGridMenu {
  public:
	CraftingMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos);
	ItemStack quickMoveStack(int slot) override;
	// stillValid(access, player, CRAFTING_TABLE): the table is still there, within reach
	bool	  stillValid() override;
	void	  removed() override;

  private:
	PlayerContainer _inventory;
	SimpleContainer _craft{9};
	BlockPos		_pos;
	int				_table;
};

// Opening and closing menus for a player (ServerPlayer.openMenu, closeContainer...)
namespace Menus {
	// The player's inventory menu (made the first time)
	Menu& inventory(Player& player, Level& level);
	// The menu the player has open: the inventory's when none
	Menu& current(Player& player, Level& level);
	// Opens a menu (after closing any other): its window, with this title (a text component as network NBT)
	void  open(Player& player, Level& level, std::unique_ptr<Menu> menu, const std::vector<uint8_t>& title);
	// Opens a container's menu: ContainerMenu of that type, titled with its custom name or this translation key
	void  openContainer(Player& player, Level& level, std::shared_ptr<Container> container, const std::string& type,
						const std::vector<uint8_t>& customName, const std::string& defaultName, Slot::Kind kind = Slot::Kind::Normal);
	// Opens a crafting table's menu at pos
	void  openCrafting(Player& player, Level& level, const BlockPos& pos);
	// closeContainer: the client is told, then doCloseContainer
	void  closeContainer(Player& player, Level& level);
	// doCloseContainer: the menu closes on the server (the client closed it)
	void  doCloseContainer(Player& player, Level& level);
	// Each tick: the changes to the client; a menu that isn't valid anymore closes
	void  tick(Player& player, Level& level);
} // namespace Menus

#endif
