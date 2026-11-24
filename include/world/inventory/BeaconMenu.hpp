#ifndef BEACON_MENU_HPP
#define BEACON_MENU_HPP

#include "world/blockentity/BeaconBlockEntity.hpp"
#include "world/inventory/Menu.hpp"

// BeaconMenu: the payment slot (one of #beacon_payment_items), the player's inventory, and the beacon's levels and
// powers as data slots. Choosing powers (Set Beacon) takes the payment
class BeaconMenu : public Menu {
  public:
	BeaconMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos);

	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override;
	void	  removed() override;
	// BeaconMenu.updateEffects: with a payment in its slot, the powers are set and one payment is used
	bool	  updateEffects(BeaconBlockEntity::Power primary, BeaconBlockEntity::Power secondary);

  protected:
	int dataSlot(int index) const override;

  private:
	PlayerContainer _inventory;
	SimpleContainer _payment{1};
	BlockPos		_pos;

	BeaconBlockEntity* beacon() const;
	bool			   isPayment(const ItemStack& stack) const;
};

namespace Menus {
	// Opens the beacon's menu at pos
	void openBeacon(Player& player, Level& level, const BlockPos& pos);
} // namespace Menus

#endif
