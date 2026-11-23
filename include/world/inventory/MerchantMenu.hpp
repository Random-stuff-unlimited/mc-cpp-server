#ifndef MERCHANT_MENU_HPP
#define MERCHANT_MENU_HPP

#include "world/inventory/Menu.hpp"

class AbstractVillager;

// The merchant's menu (vanilla's MerchantMenu, minecraft:merchant): two payment slots and the result of the selected
// (or first matching) offer, then the player's inventory. Taking the result pays and counts a use of the offer
class MerchantMenu : public Menu {
  public:
	MerchantMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, AbstractVillager& merchant);

	ItemStack quickMoveStack(int slot) override;
	bool	  stillValid() override;
	void	  removed() override;
	void	  onTake(Slot& slot, const ItemStack& stack) override;
	// SELECT_TRADE: the offer clicked in the list; its payments come from the inventory
	void	  selectTrade(int index);
	int		  merchantId() const { return _merchantId; }

  private:
	PlayerContainer _inventory;
	SimpleContainer _payments{2};
	ResultContainer _result;
	int				_merchantId;
	int				_selectionHint = -1, _activeOffer = -1;

	AbstractVillager* merchant();
	void			  updateResult();
	// MerchantMenu.moveFromInventoryToPaymentSlot: what the offer asks for, from the inventory
	void			  moveFromInventoryToPaymentSlot(int paymentSlot, int item, int count);
};

namespace Menus {
	// Opens the merchant's menu for its offers (after closing any other), and sends the offers (MERCHANT_OFFERS)
	void openMerchant(Player& player, Level& level, AbstractVillager& merchant);
	// MERCHANT_OFFERS again (after a trade, a level up)
	void sendMerchantOffers(Player& player, Level& level, AbstractVillager& merchant, int containerId);
} // namespace Menus

#endif
