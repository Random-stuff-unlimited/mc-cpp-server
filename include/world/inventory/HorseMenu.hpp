#ifndef HORSE_MENU_HPP
#define HORSE_MENU_HPP

#include "world/inventory/Menu.hpp"

class AbstractHorse;

// HorseInventoryMenu: a horse's (donkey's, llama's...) saddle and body slots, its chest if it has one, then the
// player's inventory. Opened with HORSE_SCREEN_OPEN (the client builds it from the horse)
class HorseMenu : public Menu {
  public:
	HorseMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, AbstractHorse& horse);

	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override;
	int		  horseId() const { return _horseId; }

  private:
	// The horse's saddle and body slots as a container
	class EquipmentContainer : public Container {
	  public:
		explicit EquipmentContainer(AbstractHorse& horse) : _horse(horse) {}
		int		   size() const override { return 2; }
		ItemStack& item(int slot) override;
		int		   maxStackSize() const override { return 1; }

	  private:
		AbstractHorse& _horse;
	};

	PlayerContainer	   _inventory;
	EquipmentContainer _equipment;
	int				   _horseId, _chestSlots = 0;
};

namespace Menus {
	// Opens a horse's inventory (after closing any other menu)
	void openHorseInventory(Player& player, Level& level, AbstractHorse& horse);
} // namespace Menus

#endif
