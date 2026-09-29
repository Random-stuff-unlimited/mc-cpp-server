#include "world/inventory/AnvilMenu.hpp"

#include "lib/nbt.hpp"
#include "lib/nbtParser.hpp"
#include "lib/nbtWriter.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Xp.hpp"
#include "world/item/Components.hpp"
#include "world/item/Enchantments.hpp"
#include "world/item/PlayerInventory.hpp"

#include <algorithm>
#include <cmath>

namespace {
	// A VarInt component (minecraft:damage, minecraft:max_damage, minecraft:repair_cost), 0 if absent
	int intComponent(const ItemStack& stack, const GameData& gameData, const char* name) {
		std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, name);
		if (!value) return 0;
		size_t pos = 0;
		int	   out = 0;
		for (int shift = 0; shift < 35; shift += 7) {
			if (pos >= value->size()) return 0;
			uint8_t byte = (*value)[pos++];
			out |= static_cast<int>(byte & 0x7F) << shift;
			if (!(byte & 0x80)) return out;
		}
		return 0;
	}
	void setIntComponent(ItemStack& stack, const GameData& gameData, const char* name, int value) {
		Buffer encoded;
		encoded.writeVarInt(value);
		Components::set(stack, gameData, name, encoded.getData());
	}
	int repairCost(const ItemStack& stack, const GameData& gameData) { return intComponent(stack, gameData, "minecraft:repair_cost"); }
	int damage(const ItemStack& stack, const GameData& gameData) { return intComponent(stack, gameData, "minecraft:damage"); }
	// The stack's max damage: the item's durability, or an explicit minecraft:max_damage component
	int maxDamage(const ItemStack& stack, const GameData& gameData) {
		int explicit_ = intComponent(stack, gameData, "minecraft:max_damage");
		if (explicit_ > 0) return explicit_;
		const GameData::ItemProperties* props = gameData.getItemProperties(stack.item);
		return props ? props->maxDamage : 0;
	}
	// Item.canRepair: the repair material (minecraft:repairable) is this stack's item
	bool canRepair(const GameData& gameData, const ItemStack& target, const ItemStack& material) {
		const GameData::ItemProperties* props = gameData.getItemProperties(target.item);
		if (!props || props->repairItems.empty()) return false;
		const std::string& repair = props->repairItems;
		if (repair[0] == '#') return gameData.isInTag("minecraft:item", repair.substr(1), material.item);
		return gameData.getStaticId("minecraft:item", repair) == material.item;
	}
	// The level of an enchantment in a list, 0 if absent
	int levelOf(const std::vector<std::pair<int, int>>& enchantments, int id) {
		for (auto [existing, level] : enchantments) {
			if (existing == id) return level;
		}
		return 0;
	}
	// Enchantment.isAcceptableItem: the enchantment can go on this item (its primary items), books take everything
	bool canEnchantItem(const GameData& gameData, int enchantment, const ItemStack& stack) {
		const nlohmann::json* definition = Enchantments::definition(gameData, enchantment);
		if (!definition) return false;
		std::string item = gameData.getStaticName("minecraft:item", stack.item);
		bool		book = item == "minecraft:book" || item == "minecraft:enchanted_book";
		if (book) return true;
		const nlohmann::json& items = definition->contains("primary_items") ? definition->at("primary_items") : definition->at("supported_items");
		// A #tag or an item name (Enchantments::selectEnchantment's itemsMatch, inlined for a single item)
		if (items.is_array()) {
			for (const auto& entry : items) {
				std::string name = entry.get<std::string>();
				if (name[0] == '#' ? gameData.isInTag("minecraft:item", name.substr(1), stack.item) : gameData.getStaticId("minecraft:item", name) == stack.item)
					return true;
			}
			return false;
		}
		std::string name = items.get<std::string>();
		return name[0] == '#' ? gameData.isInTag("minecraft:item", name.substr(1), stack.item) : gameData.getStaticId("minecraft:item", name) == stack.item;
	}
	// The stack's custom name as text ("" if it has none)
	std::string customName(const ItemStack& stack, const GameData& gameData) {
		std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, "minecraft:custom_name");
		if (!value) return "";
		size_t		cursor = 0;
		nbt::Tag	tag	= nbt::Parser().parseNetwork(*value, cursor);
		if (const auto* text = std::get_if<nbt::TagString>(&tag.data)) return *text;
		if (const auto* compoundPtr = std::get_if<std::shared_ptr<nbt::TagCompound>>(&tag.data)) {
			const nbt::TagCompound& compound = **compoundPtr;
			auto it = compound.data.find("text");
			if (it != compound.data.end()) {
				if (const auto* text = std::get_if<nbt::TagString>(&it->second.data)) return *text;
			}
		}
		return "";
	}
	bool hasCustomName(const ItemStack& stack, const GameData& gameData) { return !customName(stack, gameData).empty(); }
	// Writes a literal text component as the custom name
	void setCustomName(ItemStack& stack, const GameData& gameData, const std::string& name) {
		std::vector<uint8_t> bytes;
		nbt::Writer::writeNetwork(nbt::Tag(nbt::TagString(name)), bytes);
		Components::set(stack, gameData, "minecraft:custom_name", std::move(bytes));
	}
	void removeCustomName(ItemStack& stack, const GameData& gameData) { Components::set(stack, gameData, "minecraft:custom_name", std::nullopt); }
	bool blank(const std::string& name) {
		for (char c : name) {
			if (c != ' ') return false;
		}
		return true;
	}
	// AnvilScreenHandler.getNextCost: a result's repair cost after an operation
	int nextCost(int cost) { return static_cast<int>(std::min(static_cast<int64_t>(cost) * 2 + 1, static_cast<int64_t>(2147483647))); }
} // namespace

ItemStack AnvilMenu::Result::removeItem(int slot, int count) {
	if (allowed && !allowed()) return {};
	return ResultContainer::removeItem(slot, count);
}

AnvilMenu::AnvilMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos)
	: Menu(level, player, containerId, "minecraft:anvil"), _inventory(inventory), _pos(pos),
	  _anvil(level.gameData().getStaticId("minecraft:block", "minecraft:anvil")),
	  _chippedAnvil(level.gameData().getStaticId("minecraft:block", "minecraft:chipped_anvil")),
	  _damagedAnvil(level.gameData().getStaticId("minecraft:block", "minecraft:damaged_anvil")),
	  _enchantedBook(level.gameData().getStaticId("minecraft:item", "minecraft:enchanted_book")) {
	addSlot(_inputs, 0); // The item to repair, combine or rename
	addSlot(_inputs, 1); // The material, item or enchanted book
	addSlot(_result, 0, Slot::Kind::Result);
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	addDataSlots(1);
	// The result follows the inputs (and the typed name)
	_inputs.changed = [this] { updateResult(); };
	_result.allowed = [this] {
		bool creative = _player.getGameMode() == GameMode::Creative;
		return _levelCost > 0 && (creative || (_levelCost < 40 && _player.getXpLevel() >= _levelCost));
	};
	updateResult();
}

void AnvilMenu::updateResult() {
	ItemStack& left	 = _inputs.item(0);
	ItemStack& right = _inputs.item(1);
	bool	   creative = _player.getGameMode() == GameMode::Creative;

	_levelCost = 1;
	_repairItemUsage = 0;
	int i = 0; // The cost of the operations (repairs, combines, rename)
	int l = 0; // The base repair cost of the two items
	int j = 0; // The cost of a rename alone

	const GameData::ItemProperties* leftProps = left.isEmpty() ? nullptr : _gameData.getItemProperties(left.item);
	if (!left.isEmpty() && leftProps && (maxDamage(left, _gameData) > 0 || leftProps->enchantability > 0)) {
		ItemStack out = left;
		std::vector<std::pair<int, int>> enchantments = Enchantments::of(_gameData, out);
		l += repairCost(left, _gameData) + repairCost(right, _gameData);

		if (!right.isEmpty()) {
			bool book = right.item == _enchantedBook;
			if (maxDamage(out, _gameData) > 0 && canRepair(_gameData, left, right)) {
				// Repair with the material: a quarter of the max damage per material item
				int k = std::min(damage(out, _gameData), maxDamage(out, _gameData) / 4);
				if (k <= 0) {
					_result.setItem(0, ItemStack());
					_levelCost = 0;
					return;
				}
				int m;
				for (m = 0; k > 0 && m < right.count; m++) {
					setIntComponent(out, _gameData, "minecraft:damage", damage(out, _gameData) - k);
					i++;
					k = std::min(damage(out, _gameData), maxDamage(out, _gameData) / 4);
				}
				_repairItemUsage = m;
			} else {
				// Combine: two of the same damaged item, or an enchanted book's enchantments
				if (!book && (out.item != right.item || maxDamage(out, _gameData) <= 0)) {
					_result.setItem(0, ItemStack());
					_levelCost = 0;
					return;
				}
				if (maxDamage(out, _gameData) > 0 && !book) {
					// The two damaged tools merge their remaining durability, +12% of the max
					int kx = maxDamage(left, _gameData) - damage(left, _gameData);
					int m	= maxDamage(right, _gameData) - damage(right, _gameData);
					int n	= m + maxDamage(left, _gameData) * 12 / 100;
					int o	= kx + n;
					int p	= maxDamage(left, _gameData) - o;
					if (p < 0) p = 0;
					if (p < damage(out, _gameData)) {
						setIntComponent(out, _gameData, "minecraft:damage", p);
						i += 2;
					}
				}

				// The right item's enchantments, added or leveled up on the left (the costs of an enchanted book are halved)
				bool any		 = false;
				bool incompatible = false;
				for (auto [enchantment, level] : Enchantments::of(_gameData, right)) {
					int q = levelOf(enchantments, enchantment);
					level = q == level ? level + 1 : std::max(level, q);
					bool acceptable = canEnchantItem(_gameData, enchantment, left);
					if (creative || left.item == _enchantedBook) acceptable = true;
					for (auto [existing, existingLevel] : enchantments) {
						if (existing != enchantment && !Enchantments::areCompatible(_gameData, enchantment, existing)) {
							acceptable = false;
							i++;
						}
					}
					if (!acceptable) {
						incompatible = true;
					} else {
						any = true;
						const nlohmann::json* definition = Enchantments::definition(_gameData, enchantment);
						int maxLevel = definition ? definition->value("max_level", 1) : 1;
						if (level > maxLevel) level = maxLevel;
						auto it = std::find_if(enchantments.begin(), enchantments.end(), [&](const auto& e) { return e.first == enchantment; });
						if (it == enchantments.end()) {
							enchantments.emplace_back(enchantment, level);
						} else {
							it->second = level;
						}
						int cost = definition ? definition->value("anvil_cost", 1) : 1;
						if (book) cost = std::max(1, cost / 2);
						i += cost * level;
						if (left.count > 1) i = 40;
					}
				}
				if (incompatible && !any) {
					_result.setItem(0, ItemStack());
					_levelCost = 0;
					return;
				}
			}
		}

		// Rename: the typed name, or removing the current one
		if (!_newItemName.empty() && !blank(_newItemName)) {
			if (_newItemName != customName(left, _gameData)) {
				j = 1;
				i += j;
				setCustomName(out, _gameData, _newItemName);
			}
		} else if (hasCustomName(left, _gameData)) {
			j = 1;
			i += j;
			removeCustomName(out, _gameData);
		}

		_levelCost = static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(l) + i, 0, 2147483647));
		if (i <= 0) {
			out = ItemStack();
		}
		// A rename alone caps at 39, everything else at 39 too (Too Expensive in survival)
		if (j == i && j > 0 && _levelCost >= 40) _levelCost = 39;
		if (_levelCost >= 40 && !creative) out = ItemStack();

		if (!out.isEmpty()) {
			int cost = repairCost(out, _gameData);
			if (cost < repairCost(right, _gameData)) cost = repairCost(right, _gameData);
			if (j != i || j == 0) cost = nextCost(cost);
			setIntComponent(out, _gameData, "minecraft:repair_cost", cost);
			Enchantments::set(_gameData, out, enchantments);
		}
		_result.setItem(0, std::move(out));
	} else {
		_result.setItem(0, ItemStack());
		_levelCost = 0;
	}
}

int AnvilMenu::dataSlot(int index) const {
	return index == 0 ? _levelCost : 0;
}

bool AnvilMenu::stillValid() {
	int block = _gameData.getBlockOfState(_level.getBlockState(_pos));
	if (block != _anvil && block != _chippedAnvil && block != _damagedAnvil) return false;
	// Player.canInteractWithBlock(pos, 4.0): the block's box within the interaction range + 4
	double range = (_player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double eyeY	 = _player.getY() + 1.62;
	double dx	 = std::max({_pos.x - _player.getX(), 0.0, _player.getX() - (_pos.x + 1.0)});
	double dy	 = std::max({_pos.y - eyeY, 0.0, eyeY - (_pos.y + 1.0)});
	double dz	 = std::max({_pos.z - _player.getZ(), 0.0, _player.getZ() - (_pos.z + 1.0)});
	return dx * dx + dy * dy + dz * dz < range * range;
}

void AnvilMenu::renameItem(const std::string& name) {
	// Sanitize like vanilla's StringHelper.stripInvalidChars: the section sign, the control characters and DEL out
	std::string sanitized;
	for (unsigned char c : name) {
		if (c != 167 && c >= 32 && c != 127) sanitized += static_cast<char>(c);
	}
	if (sanitized.size() > 50) return;
	if (sanitized == _newItemName) return;
	_newItemName = sanitized;
	updateResult();
}

void AnvilMenu::onTake(Slot& slot, const ItemStack& stack) {
	if (slot.kind != Slot::Kind::Result) {
		Menu::onTake(slot, stack);
		return;
	}
	bool creative = _player.getGameMode() == GameMode::Creative;
	// onTakeOutput: the levels, then the inputs, then the anvil may break
	if (!creative) Xp::addLevels(_level.server(), _player, -_levelCost);

	_inputs.setItem(0, ItemStack());
	if (_repairItemUsage > 0) {
		ItemStack& right = _inputs.item(1);
		if (!right.isEmpty() && right.count > _repairItemUsage) {
			right.shrink(_repairItemUsage);
			_inputs.setItem(1, right);
		} else {
			_inputs.setItem(1, ItemStack());
		}
	} else {
		_inputs.setItem(1, ItemStack());
	}
	_levelCost = 0;

	// AnvilBlock: a 12% chance to damage it, breaking it once it is already damaged (not in creative)
	int block = _gameData.getBlockOfState(_level.getBlockState(_pos));
	if (!creative && _level.random().nextFloat() < 0.12F) {
		if (block == _anvil) {
			_level.setBlock(_pos, _gameData.getDefaultBlockState("minecraft:chipped_anvil"), Level::UPDATE_CLIENTS);
			_level.playSound(nullptr, _pos, "minecraft:block.anvil.use", Level::SoundSource::Blocks);
		} else if (block == _chippedAnvil) {
			_level.setBlock(_pos, _gameData.getDefaultBlockState("minecraft:damaged_anvil"), Level::UPDATE_CLIENTS);
			_level.playSound(nullptr, _pos, "minecraft:block.anvil.use", Level::SoundSource::Blocks);
		} else if (block == _damagedAnvil) {
			_level.removeBlock(_pos, false);
			_level.playSound(nullptr, _pos, "minecraft:block.anvil.destroy", Level::SoundSource::Blocks);
		} else {
			_level.playSound(nullptr, _pos, "minecraft:block.anvil.use", Level::SoundSource::Blocks);
		}
	} else {
		_level.playSound(nullptr, _pos, "minecraft:block.anvil.use", Level::SoundSource::Blocks);
	}
	updateResult();
}

// The result: into the inventory, hotbar first. Inputs: into the inventory. Inventory: lapis... nothing special here:
// items move between the inventory and the two inputs (any item in the first, any item in the second)
ItemStack AnvilMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index == 2) {
		if (!moveItemStackTo(stack, 3, 39, true)) return {};
		slot.set(ItemStack());
	} else if (index == 0 || index == 1) {
		if (!moveItemStackTo(stack, 3, 39, true)) return {};
	} else {
		// From the inventory: into the first input if it is empty, else the second
		int target = _inputs.item(0).isEmpty() ? 0 : (_inputs.item(1).isEmpty() ? 1 : -1);
		if (target < 0) return {};
		ItemStack one = stack.copyWithCount(1);
		stack.shrink(1);
		_inputs.setItem(target, std::move(one));
	}
	if (stack.isEmpty()) {
		slot.set(ItemStack());
	} else {
		slot.setChanged();
	}
	if (stack.count == original.count) return {};
	ItemStack left = stack;
	onTake(slot, left);
	return original;
}

void AnvilMenu::removed() {
	Menu::removed();
	clearContainer(_inputs);
	_result.removeItemNoUpdate(0);
}

namespace Menus {
	void openAnvil(Player& player, Level& level, const BlockPos& pos) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int		id	  = player.nextContainerCounter();
		Buffer	title;
		TextComponent::writeTranslatable(title, "container.repair", {});
		open(player, level, std::make_unique<AnvilMenu>(level, player, player.inventory(), id, pos), title.getData());
	}
} // namespace Menus