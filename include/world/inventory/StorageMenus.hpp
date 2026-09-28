#ifndef STORAGE_MENUS_HPP
#define STORAGE_MENUS_HPP

#include "world/blockentity/StorageEntities.hpp"
#include "world/inventory/Menu.hpp"

#include <memory>

// The menus of lecterns and crafters (their block entities are in world/blockentity/StorageEntities.hpp)

// LecternMenu: the book's slot and its page (data slot 0). Buttons: 1 previous page, 2 next page, 3 take the book,
// 100 + n go to page n
class LecternMenu : public Menu {
  public:
	LecternMenu(Level& level, Player& player, int containerId, std::shared_ptr<BlockEntity> lectern);
	ItemStack quickMoveStack(int) override { return {}; }
	bool	  stillValid() override { return _lectern->bookAccess().stillValid(_player); }
	bool	  clickMenuButton(int button) override;

  protected:
	int dataSlot(int index) const override { return index == 0 ? _lectern->page() : 0; }

  private:
	std::shared_ptr<BlockEntity> _keepAlive;
	LecternBlockEntity*			 _lectern;
	// setData: the page, then the changes to the client right away
	void						 setPage(int page);
};

// CrafterMenu: the crafter's 3x3 grid (CrafterSlot), the inventory, then the result it would craft (only shown); data
// slots 0-8 the disabled slots, 9 whether it is powered
class CrafterMenu : public Menu {
  public:
	CrafterMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, std::shared_ptr<BlockEntity> crafter);
	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override { return _crafter->stillValid(_player); }
	CrafterBlockEntity& crafter() { return *_crafter; }

  protected:
	int	 dataSlot(int index) const override { return _crafter->data(index); }
	// slotChanged: any slot changed, the result is looked for again
	void beforeBroadcastChanges() override;

  private:
	PlayerContainer				 _inventory;
	std::shared_ptr<BlockEntity> _keepAlive;
	CrafterBlockEntity*			 _crafter;
	ResultContainer				 _result;
	std::vector<ItemStack>		 _lastSlots;
	void						 refreshRecipeResult();
};

namespace Menus {
	// Opens a menu made with its container id (after closing the open one), titled with this translation key or
	// custom name
	void openStorage(Player& player, Level& level, const std::function<std::unique_ptr<Menu>(int)>& create, const std::vector<uint8_t>& customName,
					 const std::string& defaultName);
} // namespace Menus

#endif
