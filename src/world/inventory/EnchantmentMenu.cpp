#include "world/inventory/EnchantmentMenu.hpp"

#include "lib/JavaRandom.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Xp.hpp"
#include "world/item/Enchantments.hpp"
#include "world/item/PlayerInventory.hpp"

#include <algorithm>
#include <cmath>

namespace {
	// EnchantingTableBlock.POWER_PROVIDER_OFFSETS: the 33 positions around the table that can hold a bookshelf
	constexpr int OFFSETS[33][3] = {{-2, 0, 0}, {2, 0, 0},	{0, 0, -2},  {0, 0, 2},	  {-2, 1, 0}, {2, 1, 0},	 {0, 1, -2},  {0, 1, 2},
									{-2, 2, 0}, {2, 2, 0},	{0, 2, -2},  {0, 2, 2},	  {-2, 0, -2}, {2, 0, -2}, {-2, 0, 2},  {2, 0, 2},
									{-2, 1, -2}, {2, 1, -2}, {-2, 1, 2},  {2, 1, 2},	  {-2, 2, -2}, {2, 2, -2}, {-2, 2, 2},  {2, 2, 2},
									{0, 2, 0}, {1, 2, 0},	{-1, 2, 0},  {0, 2, 1},	  {0, 2, -1},  {0, 1, 1},	 {0, 1, -1},  {1, 1, 0},
									{-1, 1, 0}};

	// EnchantmentHelper.calculateRequiredExperienceLevel: the level an option requires (shown in the menu; the cost
	// paid is only 1-3 levels). `slot` is 0-2, `bookshelves` is capped at 15
	int costFor(JavaRandom& random, int slot, int bookshelves, int enchantability) {
		if (enchantability <= 0) return 0;
		if (bookshelves > 15) bookshelves = 15;
		int j = random.nextInt(8) + 1 + (bookshelves >> 1) + random.nextInt(bookshelves + 1);
		if (slot == 0) return std::max(j / 3, 1);
		if (slot == 1) return j * 2 / 3 + 1;
		return std::max(j, bookshelves * 2);
	}
} // namespace

EnchantmentMenu::EnchantmentMenu(Level& level, Player& player, PlayerInventory& inventory, int containerId, const BlockPos& pos)
	: Menu(level, player, containerId, "minecraft:enchantment"), _inventory(inventory), _pos(pos),
	  _enchantingTable(level.gameData().getStaticId("minecraft:block", "minecraft:enchanting_table")),
	  _bookshelf(level.gameData().getStaticId("minecraft:block", "minecraft:bookshelf")),
	  _lapisLazuli(level.gameData().getStaticId("minecraft:item", "minecraft:lapis_lazuli")) {
	// The item (one at a time) and the lapis lazuli
	addSlot(_enchant, 0).stackLimit = [](const ItemStack&) { return 1; };
	addSlot(_enchant, 1).placeRule = [this](const ItemStack& stack) { return stack.item == _lapisLazuli; };
	for (int i = 9; i < 36; i++) addSlot(_inventory, i);
	for (int i = 0; i < 9; i++) addSlot(_inventory, i);
	addDataSlots(10);
	// The offers follow the item: regenerated whenever the two slots change (same seed, so the same rolls)
	_enchant.changed = [this] { refresh(); };
	refresh();
}

void EnchantmentMenu::refresh() {
	ItemStack& item = _enchant.item(0);
	const GameData::ItemProperties* props = item.isEmpty() ? nullptr : _gameData.getItemProperties(item.item);
	// ItemStack.isEnchantable: has an enchantable component and isn't already enchanted
	bool valid = props && props->enchantability > 0 && Enchantments::of(_gameData, item).empty();
	if (!valid) {
		for (int i = 0; i < 3; i++) {
			_costs[i] = 0;
			_clue[i]  = -1;
			_levels[i] = -1;
		}
		return;
	}

	// The bookshelves that can power the table (EnchantingTableBlock.canAccessPowerProvider): a bookshelf at the
	// offset, and air halfway to the table
	int bookshelves = 0;
	for (const auto& [dx, dy, dz] : OFFSETS) {
		if (_level.blocks().blockOf(_level.getBlockState(_pos.offset(dx, dy, dz))) != _bookshelf) continue;
		if (!_level.blocks().isAir(_level.getBlockState(_pos.offset(dx / 2, dy, dz / 2)))) continue;
		bookshelves++;
	}

	// One seeded stream for the costs, then one per slot for its enchantments (vanilla EnchantmentMenu.slotsChanged)
	int enchantability = props->enchantability;
	JavaRandom random(_player.getXpSeed());
	random.setSeed(_player.getXpSeed());
	for (int i = 0; i < 3; i++) {
		_costs[i] = costFor(random, i, bookshelves, enchantability);
		_clue[i]  = -1;
		_levels[i] = -1;
		if (_costs[i] < i + 1) _costs[i] = 0; // The option needs at least its lapis cost in levels
	}
	const std::vector<int> candidates = Enchantments::tagEntries(_gameData, "minecraft:in_enchanting_table");
	bool book = _gameData.getStaticName("minecraft:item", item.item) == "minecraft:book";
	for (int i = 0; i < 3; i++) {
		if (_costs[i] <= 0) continue;
		random.setSeed(static_cast<int64_t>(_player.getXpSeed()) + i);
		std::vector<std::pair<int, int>> rolled = Enchantments::selectEnchantment(_gameData, random, item, _costs[i], candidates);
		if (rolled.empty()) continue;
		// A book keeps only one of the enchantments it rolled
		if (book && rolled.size() > 1) rolled.erase(rolled.begin() + random.nextInt(static_cast<int>(rolled.size())));
		auto [id, level] = rolled[random.nextInt(static_cast<int>(rolled.size()))];
		_clue[i]  = id;
		_levels[i] = level;
	}
}

int EnchantmentMenu::dataSlot(int index) const {
	switch (index) {
	case 0:
	case 1:
	case 2:
		return _costs[index];
	case 3:
		return _player.getXpSeed();
	case 4:
	case 5:
	case 6:
		return _clue[index - 4];
	case 7:
	case 8:
	case 9:
		return _levels[index - 7];
	default:
		return 0;
	}
}

bool EnchantmentMenu::stillValid() {
	if (_gameData.getBlockOfState(_level.getBlockState(_pos)) != _enchantingTable) return false;
	// Player.canInteractWithBlock(pos, 4.0): the block's box within the interaction range + 4
	double range = (_player.getGameMode() == GameMode::Creative ? 5.0 : 4.5) + 4.0;
	double eyeY	 = _player.getY() + 1.62;
	double dx	 = std::max({_pos.x - _player.getX(), 0.0, _player.getX() - (_pos.x + 1.0)});
	double dy	 = std::max({_pos.y - eyeY, 0.0, eyeY - (_pos.y + 1.0)});
	double dz	 = std::max({_pos.z - _player.getZ(), 0.0, _player.getZ() - (_pos.z + 1.0)});
	return dx * dx + dy * dy + dz * dz < range * range;
}

bool EnchantmentMenu::clickMenuButton(int id) {
	if (id < 0 || id >= 3) return false;
	ItemStack& item	 = _enchant.item(0);
	ItemStack& lapis = _enchant.item(1);
	int		   cost	 = id + 1; // Levels and lapis, by position (1, 2 or 3)
	bool	   creative = _player.getGameMode() == GameMode::Creative;
	if (!creative && (lapis.isEmpty() || lapis.count < cost)) return false;
	if (_costs[id] <= 0 || item.isEmpty() || (!creative && (_player.getXpLevel() < cost || _player.getXpLevel() < _costs[id]))) return false;

	// The enchantments of this option, re-rolled with the same seed (vanilla's generateEnchantments on click)
	JavaRandom random(_player.getXpSeed());
	random.setSeed(static_cast<int64_t>(_player.getXpSeed()) + id);
	const std::vector<int> candidates = Enchantments::tagEntries(_gameData, "minecraft:in_enchanting_table");
	std::vector<std::pair<int, int>> rolled = Enchantments::selectEnchantment(_gameData, random, item, _costs[id], candidates);
	if (rolled.empty()) return false;
	bool book = _gameData.getStaticName("minecraft:item", item.item) == "minecraft:book";
	if (book && rolled.size() > 1) rolled.erase(rolled.begin() + random.nextInt(static_cast<int>(rolled.size())));

	// Player.applyEnchantmentCosts: the cost in levels (id + 1), and the seed changes after
	Xp::addLevels(_level.server(), _player, -cost);
	_player.setXpSeed(_level.random().nextInt());

	// A book becomes an enchanted book, then every enchantment rolled is applied (upgrading what's already there)
	int bookItem	 = _gameData.getStaticId("minecraft:item", "minecraft:book");
	int enchantedBook = _gameData.getStaticId("minecraft:item", "minecraft:enchanted_book");
	if (item.item == bookItem) item = ItemStack(enchantedBook, item.count);
	std::vector<std::pair<int, int>> current = Enchantments::of(_gameData, item);
	for (auto [enchantment, level] : rolled) {
		auto it = std::find_if(current.begin(), current.end(), [&](const auto& e) { return e.first == enchantment; });
		if (it == current.end()) {
			current.emplace_back(enchantment, level);
		} else {
			it->second = std::max(it->second, level);
		}
	}
	Enchantments::set(_gameData, item, current);

	// The lapis (not in creative), then the offers roll again with the new seed
	if (!creative) {
		lapis.shrink(cost);
		if (lapis.isEmpty()) lapis = ItemStack();
	}
	refresh();
	_level.playSoundAt(nullptr, _pos.x + 0.5, _pos.y + 0.5, _pos.z + 0.5, "minecraft:block.enchantment_table.use", Level::SoundSource::Blocks, 1.0F,
					   _level.random().nextFloat() * 0.1F + 0.9F);
	return true;
}

// Result: into the inventory, hotbar first. Inventory: lapis into its slot, anything else (one at a time) into the
// item slot. Item and lapis: into the inventory
ItemStack EnchantmentMenu::quickMoveStack(int index) {
	Slot& slot = _slots[index];
	if (!slot.hasItem()) return {};
	ItemStack& stack	= slot.item();
	ItemStack  original = stack;
	if (index == 0 || index == 1) {
		if (!moveItemStackTo(stack, 2, 38, true)) return {};
	} else if (stack.item == _lapisLazuli) {
		if (!moveItemStackTo(stack, 1, 2, true)) return {};
	} else {
		if (!_enchant.item(0).isEmpty()) return {};
		ItemStack one = stack.copyWithCount(1);
		stack.shrink(1);
		_enchant.setItem(0, std::move(one));
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

void EnchantmentMenu::removed() {
	Menu::removed();
	clearContainer(_enchant);
}

namespace Menus {
	void openEnchanting(Player& player, Level& level, const BlockPos& pos) {
		if (player.openMenuSlot()) closeContainer(player, level);
		int		id	  = player.nextContainerCounter();
		Buffer	title;
		TextComponent::writeTranslatable(title, "container.enchant", {});
		open(player, level, std::make_unique<EnchantmentMenu>(level, player, player.inventory(), id, pos), title.getData());
	}
} // namespace Menus