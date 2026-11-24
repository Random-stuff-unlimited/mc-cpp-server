#include "world/inventory/HorseMenu.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/mobs/Horses.hpp"
#include "world/item/PlayerInventory.hpp"

ItemStack& HorseMenu::EquipmentContainer::item(int slot) { return _horse.equipmentSlot(slot == 0 ? EquipmentSlot::Saddle : EquipmentSlot::Body); }

HorseMenu::HorseMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, AbstractHorse& horse)
	: Menu(level, player, containerId, ""), _inventory(inventory), _equipment(horse), _horseId(horse.id()) {
	const GameData& data   = level.gameData();
	int				saddle = data.getStaticId("minecraft:item", "minecraft:saddle");
	Slot&			first  = addSlot(_equipment, 0);
	first.placeRule		   = [&horse, saddle](const ItemStack& stack) { return stack.item == saddle && horse.canUseSaddle() && horse.isTamed(); };
	Slot& body			   = addSlot(_equipment, 1);
	body.placeRule		   = [&horse](const ItemStack& stack) { return horse.canWearBody(stack); };
	if (SimpleContainer* chest = horse.chestContainer()) {
		_chestSlots = horse.inventoryColumns() * 3;
		for (int i = 0; i < _chestSlots; i++) addSlot(*chest, i);
	}
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
}

bool HorseMenu::stillValid() {
	auto* horse = dynamic_cast<AbstractHorse*>(_level.actorById(_horseId));
	return horse && horse->isAlive() && horse->distanceToSqr(_player) <= 8.0 * 8.0;
}

ItemStack HorseMenu::quickMoveStack(int index) {
	// HorseInventoryMenu.quickMoveStack: horse slots to the inventory; from the inventory, the saddle or body slot it
	// fits, else the chest, else between the inventory and the hotbar
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	 = slot.item();
	ItemStack  original	 = stack;
	int		   inventory = 2 + _chestSlots, end = inventory + 36;
	if (index < inventory) {
		if (!moveItemStackTo(stack, inventory, end, true)) return {};
	} else if (_slots[0].mayPlace(stack, _gameData) && !_slots[0].hasItem()) {
		if (!moveItemStackTo(stack, 0, 1, false)) return {};
	} else if (_slots[1].mayPlace(stack, _gameData) && !_slots[1].hasItem()) {
		if (!moveItemStackTo(stack, 1, 2, false)) return {};
	} else if (_chestSlots > 0 && moveItemStackTo(stack, 2, inventory, false)) {
		// Into the chest
	} else if (index < inventory + 27) {
		if (!moveItemStackTo(stack, inventory + 27, end, false)) return {};
	} else if (!moveItemStackTo(stack, inventory, inventory + 27, false)) {
		return {};
	}
	if (stack.isEmpty() || stack.count <= 0) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	return stack.count == original.count ? ItemStack{} : original;
}

namespace Menus {
	void openHorseInventory(Player& player, Level& level, AbstractHorse& horse) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int	   id = player.nextContainerCounter();
		Buffer packet;
		packet.writeVarInt(id);
		packet.writeVarInt(horse.inventoryColumns());
		packet.writeInt(horse.id());
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::HORSE_SCREEN_OPEN, packet, level.server());
		auto menu = std::make_unique<HorseMenu>(level, player, player.inventory(), id, horse);
		menu->sendAllDataToRemote();
		player.openMenuSlot() = std::move(menu);
	}
} // namespace Menus
