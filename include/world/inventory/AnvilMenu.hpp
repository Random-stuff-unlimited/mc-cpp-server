#ifndef ANVIL_MENU_HPP
#define ANVIL_MENU_HPP

#include "world/BlockPos.hpp"
#include "world/inventory/Menu.hpp"

#include <string>

class PlayerInventory;

// The anvil's menu (vanilla's AnvilScreenHandler): two inputs and a result. The result shows what repairing,
// combining or renaming would give, and its cost in levels (the menu's data slot 0). Taking it consumes the inputs
// and the levels, and the anvil may break. Above 39 levels it is "Too Expensive" (in survival).
class AnvilMenu : public Menu {
  public:
	AnvilMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos);

	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override;
	void	  removed() override;
	// The result was taken: the levels and inputs are consumed, the anvil may break
	void	  onTake(Slot& slot, const ItemStack& stack) override;
	// The client typed a name (Rename Item packet): the result's name and cost follow
	void	  renameItem(const std::string& name);
	int		  levelCost() const { return _levelCost; }

  protected:
	int dataSlot(int index) const override;

  private:
	PlayerContainer _inventory;
	SimpleContainer _inputs{2};
	BlockPos		_pos;
	int				_anvil, _chippedAnvil, _damagedAnvil, _enchantedBook;
	int				_levelCost		= 0;
	int				_repairItemUsage = 0;
	std::string		_newItemName;

	struct Result : public ResultContainer {
		std::function<bool()> allowed; // Whether the result may be taken (the cost is affordable)
		ItemStack removeItem(int slot, int count) override;
	};
	Result _result;

	void updateResult();
};

// Opening and closing menus for a player
namespace Menus {
	// Opens the anvil's menu at pos (after closing any other)
	void openAnvil(Player& player, Level& level, const BlockPos& pos);
} // namespace Menus

#endif