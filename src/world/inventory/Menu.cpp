#include "world/inventory/Menu.hpp"
#include "world/blockentity/StorageEntities.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "network/TextComponent.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/PlayerInventory.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace {
	// Hashed component patches are only told apart from "no change"
	void skipHashedPatch(Buffer& buf, bool& empty) {
		int added = buf.readVarInt();
		for (int i = 0; i < added; i++) {
			buf.readVarInt(); // Component type
			buf.readInt();	  // Hash
		}
		int removed = buf.readVarInt();
		for (int i = 0; i < removed; i++) buf.readVarInt();
		empty = added == 0 && removed == 0;
	}

	bool sameItem(const ItemStack& a, const ItemStack& b) { return a.item == b.item; }
	// ItemStack.matches: same item, count and components (two empty stacks match)
	bool matches(const ItemStack& a, const ItemStack& b) {
		if (a.isEmpty() || b.isEmpty()) return a.isEmpty() && b.isEmpty();
		return a.count == b.count && a.sameItemSameComponents(b);
	}
	// ItemStack.split: takes up to count from the stack
	ItemStack split(ItemStack& stack, int count) {
		int		  taken = std::min(count, stack.count);
		ItemStack piece = stack.copyWithCount(taken);
		stack.shrink(taken);
		return piece;
	}
} // namespace

// ----- Hashed stacks -----

HashedStack HashedStack::read(Buffer& buf) {
	HashedStack stack;
	if (!buf.readBool()) return stack;
	stack.empty = false;
	stack.item	= buf.readVarInt();
	stack.count = buf.readVarInt();
	skipHashedPatch(buf, stack.noComponents);
	return stack;
}

bool HashedStack::matches(const ItemStack& stack) const {
	if (empty) return stack.isEmpty();
	return !stack.isEmpty() && stack.count == count && stack.item == item && noComponents && stack.components.empty();
}

// ----- Containers over the player's inventory -----

ItemStack& PlayerContainer::item(int slot) { return _inventory.getMutable(PlayerInventory::windowSlot(slot)); }

ItemStack& InventoryRange::item(int slot) { return _inventory.getMutable(_first + slot); }

// ----- Slots -----

bool Slot::mayPlace(const ItemStack& stack, const GameData& gameData) const {
	if (kind == Kind::Result || kind == Kind::NonInteractive) return false;
	if (kind == Kind::Crafter) {
		auto* crafter = dynamic_cast<CrafterBlockEntity*>(container);
		return !crafter || !crafter->isSlotDisabled(containerSlot);
	}
	if (placeRule) return placeRule(stack); // Furnaces and brewing stands
	if (kind == Kind::NoShulkerBox) return container->canPlaceItem(containerSlot, stack); // ShulkerBoxSlot
	if (kind == Kind::Armor) {
		// LivingEntity.isEquippableInSlot
		const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
		return item && item->equipmentSlot == armor;
	}
	return true;
}

int Slot::maxStackSize(const ItemStack& stack, const GameData& gameData) const {
	const GameData::ItemProperties* item = gameData.getItemProperties(stack.item);
	if (int limit = stackLimit ? stackLimit(stack) : 0; limit > 0) return std::min(limit, item ? item->maxStackSize : 64); // Furnaces and brewing stands
	return std::min(maxStackSize(), item ? item->maxStackSize : 64);
}

void Slot::set(ItemStack stack) const {
	if (stack.count <= 0) stack = ItemStack();
	container->setItem(containerSlot, std::move(stack));
}

std::optional<ItemStack> Slot::tryRemove(int count, int decrement, const GameData& gameData) const {
	if (kind == Kind::NonInteractive) return std::nullopt;
	// allowModification: can be taken from and put back
	bool allowModification = mayPickup() && mayPlace(item(), gameData);
	if (!allowModification && decrement < item().count) return std::nullopt;
	count			= std::min(count, decrement);
	ItemStack taken = remove(count);
	if (taken.isEmpty()) return std::nullopt;
	if (item().isEmpty()) set(ItemStack());
	return taken;
}

ItemStack Slot::safeTake(int count, int decrement, const GameData& gameData) const {
	std::optional<ItemStack> taken = tryRemove(count, decrement, gameData);
	if (taken) setChanged(); // onTake
	return taken ? *taken : ItemStack();
}

void Slot::safeInsert(ItemStack& stack, int count, const GameData& gameData) const {
	if (stack.isEmpty() || !mayPlace(stack, gameData)) return;
	ItemStack& current = item();
	int		   moved   = std::min(std::min(count, stack.count), maxStackSize(stack, gameData) - (current.isEmpty() ? 0 : current.count));
	if (moved <= 0) return;
	if (current.isEmpty()) {
		set(split(stack, moved));
	} else if (current.sameItemSameComponents(stack)) {
		stack.shrink(moved);
		current.grow(moved);
		set(current);
	}
	if (stack.count <= 0) stack = ItemStack();
}

// ----- Menu -----

Menu::Menu(Level& level, Player& player, int containerId, std::string type)
	: _level(level), _player(player), _gameData(level.gameData()), _containerId(containerId), _type(std::move(type)) {}

Slot& Menu::addSlot(Container& container, int containerSlot, Slot::Kind kind) {
	Slot slot{};
	slot.container	   = &container;
	slot.containerSlot = containerSlot;
	slot.index = static_cast<int>(_slots.size());
	slot.kind  = kind;
	_slots.push_back(slot);
	_remoteSlots.emplace_back();
	return _slots.back();
}

int Menu::maxStackSize(const ItemStack& stack) const {
	const GameData::ItemProperties* item = _gameData.getItemProperties(stack.item);
	return item ? item->maxStackSize : 64;
}

bool Menu::canItemQuickReplace(const Slot* slot, const ItemStack& stack, bool ignoreCount) const {
	bool empty = !slot || !slot->hasItem();
	if (!empty && stack.sameItemSameComponents(slot->item())) return slot->item().count + (ignoreCount ? 0 : stack.count) <= maxStackSize(stack);
	return empty;
}

void Menu::clicked(int slot, int button, ClickType type) { doClick(slot, button, type); }

void Menu::doClick(int slotIndex, int button, ClickType type) {
	bool creative = _player.getGameMode() == GameMode::Creative; // hasInfiniteMaterials
	if (type == ClickType::QuickCraft) {
		// Dragging the cursor's stack over slots: start (header 0), each slot (1), end (2)
		int previous	  = _quickcraftStatus;
		_quickcraftStatus = button & 3;
		if ((previous != 1 || _quickcraftStatus != 2) && previous != _quickcraftStatus) {
			resetQuickCraft();
		} else if (_carried.isEmpty()) {
			resetQuickCraft();
		} else if (_quickcraftStatus == 0) {
			_quickcraftType = button >> 2 & 3;
			// Charitable (0) and greedy (1) always, clone (2) only with infinite materials
			if (_quickcraftType == 0 || _quickcraftType == 1 || (_quickcraftType == 2 && creative)) {
				_quickcraftStatus = 1;
				_quickcraftSlots.clear();
			} else {
				resetQuickCraft();
			}
		} else if (_quickcraftStatus == 1) {
			if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size())) return;
			Slot& slot = _slots[slotIndex];
			if (canItemQuickReplace(&slot, _carried, true) && slot.mayPlace(_carried, _gameData) &&
				(_quickcraftType == 2 || _carried.count > static_cast<int>(_quickcraftSlots.size())) &&
				std::find(_quickcraftSlots.begin(), _quickcraftSlots.end(), slotIndex) == _quickcraftSlots.end()) {
				_quickcraftSlots.push_back(slotIndex);
			}
		} else if (_quickcraftStatus == 2) {
			if (!_quickcraftSlots.empty()) {
				if (_quickcraftSlots.size() == 1) {
					int only = _quickcraftSlots.front();
					resetQuickCraft();
					doClick(only, _quickcraftType, ClickType::Pickup);
					return;
				}
				ItemStack source = _carried;
				if (source.isEmpty()) {
					resetQuickCraft();
					return;
				}
				int left  = _carried.count;
				int count = static_cast<int>(_quickcraftSlots.size());
				for (int index : _quickcraftSlots) {
					Slot& slot = _slots[index];
					if (!canItemQuickReplace(&slot, _carried, true) || !slot.mayPlace(_carried, _gameData) ||
						!(_quickcraftType == 2 || _carried.count >= count)) {
						continue;
					}
					int already = slot.hasItem() ? slot.item().count : 0;
					int max		= std::min(maxStackSize(source), slot.maxStackSize(source, _gameData));
					int place	= _quickcraftType == 0 ? static_cast<int>(std::floor(static_cast<float>(source.count) / count))
								  : _quickcraftType == 1 ? 1
								  : _quickcraftType == 2 ? maxStackSize(source)
														 : source.count;
					int total = std::min(place + already, max);
					left -= total - already;
					slot.set(source.copyWithCount(total));
				}
				source.count = left;
				_carried	 = source.count > 0 ? source : ItemStack();
			}
			resetQuickCraft();
		} else {
			resetQuickCraft();
		}
		return;
	}
	if (_quickcraftStatus != 0) {
		resetQuickCraft();
		return;
	}

	if ((type == ClickType::Pickup || type == ClickType::QuickMove) && (button == 0 || button == 1)) {
		bool primary = button == 0;
		if (slotIndex == -999) {
			// Clicked outside: drop the cursor's stack, or one of it
			if (_carried.isEmpty()) return;
			if (primary) {
				_level.dropFromPlayer(_player, _carried, true);
				_carried = ItemStack();
			} else {
				_level.dropFromPlayer(_player, split(_carried, 1), true);
				if (_carried.count <= 0) _carried = ItemStack();
			}
		} else if (type == ClickType::QuickMove) {
			if (slotIndex < 0) return;
			Slot& slot = _slots[slotIndex];
			if (!slot.mayPickup()) return;
			// Again while it moves the same item (a crafting result would be crafted again)
			ItemStack moved = quickMoveStack(slotIndex);
			while (!moved.isEmpty() && sameItem(slot.item(), moved)) moved = quickMoveStack(slotIndex);
		} else {
			if (slotIndex < 0) return;
			Slot&	  slot	   = _slots[slotIndex];
			ItemStack slotItem = slot.item();
			if (slotItem.isEmpty()) {
				if (!_carried.isEmpty()) slot.safeInsert(_carried, primary ? _carried.count : 1, _gameData);
			} else if (slot.mayPickup()) {
				if (_carried.isEmpty()) {
					int						 count = primary ? slotItem.count : (slotItem.count + 1) / 2;
					std::optional<ItemStack> taken = slot.tryRemove(count, INT_MAX, _gameData);
					if (taken) {
						_carried = *taken;
						onTake(slot, *taken);
					}
				} else if (slot.mayPlace(_carried, _gameData)) {
					if (slotItem.sameItemSameComponents(_carried)) {
						slot.safeInsert(_carried, primary ? _carried.count : 1, _gameData);
					} else if (_carried.count <= slot.maxStackSize(_carried, _gameData)) {
						// Swap the cursor and the slot
						ItemStack held = _carried;
						_carried	   = slotItem;
						slot.set(held);
					}
				} else if (slotItem.sameItemSameComponents(_carried)) {
					std::optional<ItemStack> taken = slot.tryRemove(slotItem.count, maxStackSize(_carried) - _carried.count, _gameData);
					if (taken) {
						_carried.grow(taken->count);
						onTake(slot, *taken);
					}
				}
			}
			slot.setChanged();
		}
	} else if (type == ClickType::Swap && ((button >= 0 && button < 9) || button == 40)) {
		// A number key (hotbar slot) or F (offhand, 40): swaps with the hovered slot
		if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size())) return;
		PlayerContainer inventory(_player.inventory());
		ItemStack		key		 = inventory.item(button);
		Slot&			slot	 = _slots[slotIndex];
		ItemStack		slotItem = slot.item();
		if (key.isEmpty() && slotItem.isEmpty()) return;
		if (key.isEmpty()) {
			if (slot.mayPickup()) {
				inventory.setItem(button, slotItem);
				slot.set(ItemStack());
				onTake(slot, slotItem);
			}
		} else if (slotItem.isEmpty()) {
			if (slot.mayPlace(key, _gameData)) {
				int max = slot.maxStackSize(key, _gameData);
				if (key.count > max) {
					slot.set(split(inventory.item(button), max));
				} else {
					inventory.setItem(button, ItemStack());
					slot.set(key);
				}
			}
		} else if (slot.mayPickup() && slot.mayPlace(key, _gameData)) {
			int max = slot.maxStackSize(key, _gameData);
			if (key.count > max) {
				slot.set(split(inventory.item(button), max));
				slot.setChanged();
				if (!_player.inventory().add(slotItem, _player.getSelectedSlot(), creative, _gameData)) _level.dropFromPlayer(_player, slotItem, true);
			} else {
				inventory.setItem(button, slotItem);
				slot.set(key);
				slot.setChanged();
			}
		}
	} else if (type == ClickType::Clone && creative && _carried.isEmpty() && slotIndex >= 0) {
		// Middle click in creative: a full stack of it
		if (slotIndex >= static_cast<int>(_slots.size())) return;
		Slot& slot = _slots[slotIndex];
		if (slot.hasItem()) _carried = slot.item().copyWithCount(maxStackSize(slot.item()));
	} else if (type == ClickType::Throw && _carried.isEmpty() && slotIndex >= 0) {
		// Q: one item, Ctrl+Q: the whole stack (and again while it is the same item)
		if (slotIndex >= static_cast<int>(_slots.size()) || _player.combat().dead) return;
		Slot&	  slot	 = _slots[slotIndex];
		int		  count	 = button == 0 ? 1 : slot.item().count;
		ItemStack thrown = safeTake(slot, count, INT_MAX);
		_level.dropFromPlayer(_player, thrown, true);
		if (button == 1) {
			while (!thrown.isEmpty() && sameItem(slot.item(), thrown)) {
				thrown = safeTake(slot, count, INT_MAX);
				_level.dropFromPlayer(_player, thrown, true);
			}
		}
	} else if (type == ClickType::PickupAll && slotIndex >= 0) {
		// Double click: gathers the same item from every slot into the cursor, the partial stacks first
		if (slotIndex >= static_cast<int>(_slots.size())) return;
		Slot& clicked = _slots[slotIndex];
		if (_carried.isEmpty() || (clicked.hasItem() && clicked.mayPickup())) return;
		int start = button == 0 ? 0 : static_cast<int>(_slots.size()) - 1;
		int step  = button == 0 ? 1 : -1;
		for (int pass = 0; pass < 2; pass++) {
			for (int i = start; i >= 0 && i < static_cast<int>(_slots.size()) && _carried.count < maxStackSize(_carried); i += step) {
				Slot& slot = _slots[i];
				if (!slot.hasItem() || !canItemQuickReplace(&slot, _carried, true) || !slot.mayPickup() || slot.kind == Slot::Kind::Result) continue;
				ItemStack& other = slot.item();
				if (pass == 0 && other.count == maxStackSize(other)) continue;
				ItemStack taken = slot.safeTake(other.count, maxStackSize(_carried) - _carried.count, _gameData);
				_carried.grow(taken.count);
			}
		}
	}
}

ItemStack Menu::safeTake(Slot& slot, int count, int decrement) {
	std::optional<ItemStack> taken = slot.tryRemove(count, decrement, _gameData);
	if (!taken) return {};
	onTake(slot, *taken);
	return *taken;
}

// ----- Crafting -----

void Menu::updateCraftingResult(Container& grid, int width, Container& result, const Recipe* hint) {
	std::vector<ItemStack> items(static_cast<size_t>(grid.size()));
	for (int i = 0; i < grid.size(); i++) items[i] = grid.item(i);
	CraftingInput::Positioned positioned = CraftingInput::ofPositioned(width, grid.size() / width, items);
	const Recipe*			  recipe	 = _level.recipes().getRecipeFor(RecipeType::Crafting, positioned.input, hint);
	ItemStack				  crafted	 = recipe ? recipe->assemble(positioned.input) : ItemStack();
	result.item(0)						 = crafted;
	setRemoteSlot(0, crafted);
	sendSlot(0, crafted);
}

void Menu::takeCraftingResult(Container& grid, int width) {
	std::vector<ItemStack> items(static_cast<size_t>(grid.size()));
	for (int i = 0; i < grid.size(); i++) items[i] = grid.item(i);
	CraftingInput::Positioned positioned = CraftingInput::ofPositioned(width, grid.size() / width, items);
	const CraftingInput&	  input		 = positioned.input;
	// ResultSlot.getRemainingItems: the recipe's, or every item left as it is if none matches anymore
	const Recipe*		   recipe	 = _level.recipes().getRecipeFor(RecipeType::Crafting, input);
	std::vector<ItemStack> remaining = recipe ? recipe->remainingItems(input, _gameData) : input.items;
	for (int y = 0; y < input.height; y++) {
		for (int x = 0; x < input.width; x++) {
			int		  cell		= x + positioned.left + (y + positioned.top) * width;
			ItemStack remainder = remaining[x + y * input.width];
			if (!grid.item(cell).isEmpty()) grid.removeItem(cell, 1);
			if (remainder.isEmpty()) continue;
			const ItemStack& left = grid.item(cell);
			if (left.isEmpty()) {
				grid.setItem(cell, std::move(remainder));
			} else if (left.sameItemSameComponents(remainder)) {
				remainder.grow(left.count);
				grid.setItem(cell, std::move(remainder));
			} else if (!_player.inventory().add(remainder, _player.getSelectedSlot(), false, _gameData)) {
				_level.dropFromPlayer(_player, std::move(remainder), false);
			}
		}
	}
}

bool Menu::moveItemStackTo(ItemStack& stack, int start, int end, bool reverse) {
	bool moved = false;
	int	 i	   = reverse ? end - 1 : start;
	if (maxStackSize(stack) > 1) {
		// Onto stacks of the same item first
		while (!stack.isEmpty() && (reverse ? i >= start : i < end)) {
			Slot&	   slot	 = _slots[i];
			ItemStack& other = slot.item();
			if (!other.isEmpty() && stack.sameItemSameComponents(other)) {
				int total = other.count + stack.count;
				int max	  = slot.maxStackSize(other, _gameData);
				if (total <= max) {
					stack.count = 0;
					other.count = total;
					slot.setChanged();
					moved = true;
				} else if (other.count < max) {
					stack.shrink(max - other.count);
					other.count = max;
					slot.setChanged();
					moved = true;
				}
			}
			i += reverse ? -1 : 1;
		}
	}
	if (!stack.isEmpty()) {
		// Then into the first empty slot that takes it
		i = reverse ? end - 1 : start;
		while (reverse ? i >= start : i < end) {
			Slot& slot = _slots[i];
			if (slot.item().isEmpty() && slot.mayPlace(stack, _gameData)) {
				int max = slot.maxStackSize(stack, _gameData);
				slot.set(split(stack, std::min(stack.count, max)));
				slot.setChanged();
				moved = true;
				break;
			}
			i += reverse ? -1 : 1;
		}
	}
	return moved;
}

void Menu::dropOrPlaceInInventory(ItemStack stack) {
	if (stack.isEmpty()) return;
	if (_player.isDisconnected()) {
		_level.dropFromPlayer(_player, std::move(stack), false);
		return;
	}
	// Inventory.placeItemBackInInventory: into the inventory, the rest on the ground
	_player.inventory().add(stack, _player.getSelectedSlot(), false, _gameData);
	if (!stack.isEmpty()) _level.dropFromPlayer(_player, std::move(stack), false);
}

void Menu::clearContainer(Container& container) {
	for (int i = 0; i < container.size(); i++) dropOrPlaceInInventory(container.removeItemNoUpdate(i));
}

void Menu::removed() {
	if (_carried.isEmpty()) return;
	dropOrPlaceInInventory(_carried);
	_carried = ItemStack();
}

// ----- Synchronization -----

bool Menu::RemoteSlot::matches(const ItemStack& actual) {
	if (stack) return ::matches(*stack, actual);
	if (hash && hash->matches(actual)) {
		stack = actual;
		return true;
	}
	return false;
}

void Menu::sendAllDataToRemote() {
	_synchronized = true;
	Buffer content;
	content.writeVarInt(_containerId);
	content.writeVarInt(incrementStateId());
	content.writeVarInt(static_cast<int>(_slots.size()));
	for (size_t i = 0; i < _slots.size(); i++) {
		const ItemStack& stack = _slots[i].item();
		stack.write(content);
		_remoteSlots[i].force(stack);
	}
	_carried.write(content);
	_remoteCarried.force(_carried);
	Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::CONTAINER_SET_CONTENT, content, _level.server());
	for (size_t i = 0; i < _remoteData.size(); i++) {
		_remoteData[i] = dataSlot(static_cast<int>(i));
		sendData(static_cast<int>(i), _remoteData[i]);
	}
}

void Menu::sendData(int slot, int value) {
	Buffer data;
	data.writeVarInt(_containerId);
	data.writeShort(static_cast<int16_t>(slot));
	data.writeShort(static_cast<int16_t>(value));
	Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::CONTAINER_SET_DATA, data, _level.server());
}

void Menu::sendSlot(int slot, const ItemStack& stack) {
	Buffer update;
	update.writeVarInt(_containerId);
	update.writeVarInt(incrementStateId());
	update.writeShort(static_cast<int16_t>(slot));
	stack.write(update);
	Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::CONTAINER_SET_SLOT, update, _level.server());
}

void Menu::broadcastChanges() {
	beforeBroadcastChanges();
	if (!_synchronized) {
		sendAllDataToRemote();
		return;
	}
	if (_suppressRemoteUpdates) return;
	for (size_t i = 0; i < _slots.size(); i++) {
		const ItemStack& stack = _slots[i].item();
		if (_remoteSlots[i].matches(stack)) continue;
		_remoteSlots[i].force(stack);
		sendSlot(static_cast<int>(i), stack);
	}
	if (!_remoteCarried.matches(_carried)) {
		_remoteCarried.force(_carried);
		Buffer cursor;
		_carried.write(cursor);
		Packet::send(_player.shared_from_this(), PacketId::Play::Clientbound::SET_CURSOR_ITEM, cursor, _level.server());
	}
	for (size_t i = 0; i < _remoteData.size(); i++) {
		int value = dataSlot(static_cast<int>(i));
		if (_remoteData[i] == value) continue;
		_remoteData[i] = value;
		sendData(static_cast<int>(i), value);
	}
}

void Menu::transferState(Menu& from) {
	for (size_t i = 0; i < _slots.size(); i++) {
		for (size_t j = 0; j < from._slots.size(); j++) {
			if (from._slots[j].container->storage() == _slots[i].container->storage() && from._slots[j].containerSlot == _slots[i].containerSlot) {
				_remoteSlots[i] = from._remoteSlots[j];
				break;
			}
		}
	}
}

void Menu::setRemoteSlot(int slot, const ItemStack& stack) {
	if (slot >= 0 && slot < static_cast<int>(_remoteSlots.size())) _remoteSlots[slot].force(stack);
}

void Menu::setRemoteSlotUnsafe(int slot, const HashedStack& stack) {
	if (slot < 0 || slot >= static_cast<int>(_remoteSlots.size())) return;
	_remoteSlots[slot].stack.reset();
	_remoteSlots[slot].hash = stack;
}

void Menu::setRemoteCarried(const HashedStack& stack) {
	_remoteCarried.stack.reset();
	_remoteCarried.hash = stack;
}
// ----- Opening and closing -----

namespace Menus {
	Menu& inventory(Player& player, Level& level) {
		std::unique_ptr<Menu>& menu = player.inventoryMenuSlot();
		if (!menu) menu = std::make_unique<InventoryMenu>(level, player, player.inventory());
		return *menu;
	}

	Menu& current(Player& player, Level& level) { return player.openMenuSlot() ? *player.openMenuSlot() : inventory(player, level); }

	void open(Player& player, Level& level, std::unique_ptr<Menu> menu, const std::vector<uint8_t>& title) {
		// Open Screen: the window, its type and title
		Buffer packet;
		packet.writeVarInt(menu->containerId());
		packet.writeVarInt(level.gameData().getStaticId("minecraft:menu", menu->type()));
		packet.writeBytes(title);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::OPEN_SCREEN, packet, level.server());
		menu->sendAllDataToRemote();
		player.openMenuSlot() = std::move(menu);
	}

	void openContainer(Player& player, Level& level, std::shared_ptr<Container> container, const std::string& type, const std::vector<uint8_t>& customName,
					   const std::string& defaultName, Slot::Kind kind) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int id = player.nextContainerCounter();
		std::vector<uint8_t> title = customName;
		if (title.empty()) {
			// The block's name in the client's language
			Buffer name;
			TextComponent::writeTranslatable(name, defaultName, {});
			title = name.getData();
		}
		open(player, level, std::make_unique<ContainerMenu>(level, player, player.inventory(), id, type, std::move(container), kind), title);
	}

	void openCrafting(Player& player, Level& level, const BlockPos& pos) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int	   id = player.nextContainerCounter();
		Buffer title;
		TextComponent::writeTranslatable(title, "container.crafting", {});
		open(player, level, std::make_unique<CraftingMenu>(level, player, player.inventory(), id, pos), title.getData());
	}

	void closeContainer(Player& player, Level& level) {
		Buffer close;
		close.writeVarInt(current(player, level).containerId());
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::CONTAINER_CLOSE, close, level.server());
		doCloseContainer(player, level);
	}

	void doCloseContainer(Player& player, Level& level) {
		current(player, level).removed();
		// Back to the inventory, knowing what the client saw in the menu that closes
		if (player.openMenuSlot()) {
			inventory(player, level).transferState(*player.openMenuSlot());
			player.openMenuSlot().reset();
		}
	}

	void tick(Player& player, Level& level) {
		Menu& menu = current(player, level);
		menu.broadcastChanges();
		if (!menu.stillValid()) closeContainer(player, level);
	}
} // namespace Menus
