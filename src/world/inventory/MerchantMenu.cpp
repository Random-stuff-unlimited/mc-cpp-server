#include "world/inventory/MerchantMenu.hpp"

#include "PacketIds.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/mobs/Villagers.hpp"
#include "world/item/PlayerInventory.hpp"

MerchantMenu::MerchantMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, AbstractVillager& merchant)
	: Menu(level, player, containerId, "minecraft:merchant"), _inventory(inventory), _merchantId(merchant.id()) {
	addSlot(_payments, 0);
	addSlot(_payments, 1);
	addSlot(_result, 0, Slot::Kind::Result);
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	_payments.changed = [this] { updateResult(); };
}

AbstractVillager* MerchantMenu::merchant() { return dynamic_cast<AbstractVillager*>(_level.actorById(_merchantId)); }

void MerchantMenu::updateResult() {
	// MerchantContainer.updateSellItem: the selected offer if the payments pay for it, else the first they pay for
	_activeOffer			 = -1;
	AbstractVillager* trader = merchant();
	ItemStack		  result;
	const ItemStack&  a = _payments.item(0);
	const ItemStack&  b = _payments.item(1);
	if (trader && !(a.isEmpty() && b.isEmpty())) {
		std::vector<MerchantOffer>& offers = trader->offers();
		auto pays = [&](int i) { return i >= 0 && i < static_cast<int>(offers.size()) && offers[i].satisfiedBy(a, b, _gameData); };
		int	 found = pays(_selectionHint) ? _selectionHint : -1;
		for (int i = 0; found < 0 && i < static_cast<int>(offers.size()); i++) {
			if (pays(i)) found = i;
		}
		if (found >= 0 && !offers[found].isOutOfStock()) {
			_activeOffer = found;
			result		 = offers[found].result;
		}
		trader->playSound(trader->notifyTradeSound(found >= 0), 1.0F, 1.0F);
	}
	_result.setItem(0, std::move(result));
}

void MerchantMenu::onTake(Slot& slot, const ItemStack& stack) {
	if (slot.kind != Slot::Kind::Result) {
		Menu::onTake(slot, stack);
		return;
	}
	// MerchantResultSlot.onTake: the payments are used, the merchant counts the trade
	AbstractVillager* trader = merchant();
	if (!trader || _activeOffer < 0 || _activeOffer >= static_cast<int>(trader->offers().size())) return;
	MerchantOffer& offer = trader->offers()[_activeOffer];
	ItemStack	   a	 = _payments.item(0);
	ItemStack	   b	 = _payments.item(1);
	if (offer.take(a, b, _gameData)) {
		trader->notifyTrade(offer);
		_payments.item(0) = a;
		_payments.item(1) = b;
	}
	_payments.setChanged();
	Menus::sendMerchantOffers(_player, _level, *trader, containerId());
}

ItemStack MerchantMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index == 2) {
		if (!moveItemStackTo(stack, 3, 39, true)) return {};
		slot.set(ItemStack());
		onTake(slot, original);
		return original;
	}
	if (index < 2) {
		if (!moveItemStackTo(stack, 3, 39, false)) return {};
	} else if (index < 30) {
		if (!moveItemStackTo(stack, 30, 39, false)) return {};
	} else if (!moveItemStackTo(stack, 3, 30, false)) {
		return {};
	}
	if (stack.isEmpty() || stack.count <= 0) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	return stack.count == original.count ? ItemStack{} : original;
}

bool MerchantMenu::stillValid() {
	AbstractVillager* trader = merchant();
	return trader && trader->isAlive() && trader->tradingPlayer() == &_player && trader->distanceToSqr(_player) <= 8.0 * 8.0;
}

void MerchantMenu::removed() {
	Menu::removed();
	if (AbstractVillager* trader = merchant(); trader && trader->tradingPlayer() == &_player) trader->setTradingPlayer(nullptr);
	clearContainer(_payments);
	_result.removeItemNoUpdate(0);
}

void MerchantMenu::moveFromInventoryToPaymentSlot(int paymentSlot, int item, int) {
	// Fills the payment slot up to a stack with that item from the inventory
	ItemStack& payment = _payments.item(paymentSlot);
	if (!payment.isEmpty() && payment.item != item) return;
	for (int index = 3; index < 39; index++) {
		ItemStack& stack = _slots[index].item();
		if (stack.isEmpty() || stack.item != item || !stack.components.empty()) continue;
		int max	  = maxStackSize(stack);
		int room  = payment.isEmpty() ? max : max - payment.count;
		int moved = std::min(room, stack.count);
		if (moved <= 0) break;
		if (payment.isEmpty()) {
			payment = stack.copyWithCount(moved);
		} else {
			payment.grow(moved);
		}
		stack.shrink(moved);
		if (stack.count <= 0) stack = ItemStack{};
		_slots[index].setChanged();
	}
}

void MerchantMenu::selectTrade(int index) {
	// MerchantMenu.setSelectionHint and tryMoveItems: what lies in the payment slots goes back, then the offer's costs
	_selectionHint			 = index;
	AbstractVillager* trader = merchant();
	if (!trader || index < 0 || index >= static_cast<int>(trader->offers().size())) return;
	for (int slot = 0; slot < 2; slot++) {
		ItemStack& stack = _payments.item(slot);
		if (stack.isEmpty()) continue;
		moveItemStackTo(stack, 3, 39, true);
		if (stack.count <= 0) stack = ItemStack{};
	}
	const MerchantOffer& offer = trader->offers()[index];
	if (_payments.item(0).isEmpty() && _payments.item(1).isEmpty()) {
		moveFromInventoryToPaymentSlot(0, offer.costA.item, offer.adjustedCostA(_gameData));
		if (offer.costB) moveFromInventoryToPaymentSlot(1, offer.costB->item, offer.costB->count);
	}
	_payments.setChanged();
}

namespace Menus {
	void sendMerchantOffers(Player& player, Level& level, AbstractVillager& merchant, int containerId) {
		Buffer packet;
		packet.writeVarInt(containerId);
		std::vector<MerchantOffer>& offers = merchant.offers();
		packet.writeVarInt(static_cast<int>(offers.size()));
		for (const MerchantOffer& offer : offers) offer.write(packet, level.gameData());
		packet.writeVarInt(merchant.merchantLevel());
		packet.writeVarInt(merchant.merchantXp());
		packet.writeBool(merchant.showProgressBar());
		packet.writeBool(merchant.canRestock());
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::MERCHANT_OFFERS, packet, level.server());
	}

	void openMerchant(Player& player, Level& level, AbstractVillager& merchant) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int id = player.nextContainerCounter();
		// Its name: the villager's profession ("Farmer"), the wandering trader's
		std::string key = "entity.minecraft.wandering_trader";
		if (auto* villager = dynamic_cast<Villager*>(&merchant)) {
			std::string profession = villager->professionName();
			key					   = "entity.minecraft.villager." + profession.substr(profession.find(':') + 1);
		}
		Buffer title;
		TextComponent::writeTranslatable(title, key, {});
		merchant.setTradingPlayer(&player);
		open(player, level, std::make_unique<MerchantMenu>(level, player, player.inventory(), id, merchant), title.getData());
		sendMerchantOffers(player, level, merchant, id);
	}
} // namespace Menus
