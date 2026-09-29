#ifndef ENCHANTMENT_MENU_HPP
#define ENCHANTMENT_MENU_HPP

#include "world/BlockPos.hpp"
#include "world/inventory/Menu.hpp"

class PlayerInventory;

// The enchanting table's menu (vanilla's EnchantmentMenu): an item and lapis lazuli, and three enchantment options
// whose required level, enchantment and level go to the client as the menu's data slots (0-2, 4-6, 7-9; 3 is the
// enchantment seed). Clicking an option (Container Button Click, 0-2) enchants the item.
class EnchantmentMenu : public Menu {
  public:
	EnchantmentMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos);

	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override;
	void	  removed() override;
	// Selecting an enchantment option (clickMenuButton): the lapis and XP cost, the enchantments applied
	bool	  clickMenuButton(int id) override;

  protected:
	int dataSlot(int index) const override;

  private:
	PlayerContainer _inventory;
	SimpleContainer _enchant{2};
	BlockPos		_pos;
	int				_enchantingTable, _bookshelf, _lapisLazuli;
	// The offers (vanilla's costs / enchantClue / levelClue), sent as the menu's data slots
	int _costs[3] = {0, 0, 0};
	int _clue[3]  = {-1, -1, -1};
	int _levels[3] = {-1, -1, -1};

	void refresh(); // Re-roll the three offers for the item in the slot, from the enchantment seed
};

// Opening and closing menus for a player
namespace Menus {
	// Opens the enchanting table's menu at pos (after closing any other)
	void openEnchanting(Player& player, Level& level, const BlockPos& pos);
} // namespace Menus

#endif