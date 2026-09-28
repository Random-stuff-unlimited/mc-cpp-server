#include "LevelFixture.hpp"
#include "Test.hpp"
#include "player.hpp"
#include "world/inventory/Menu.hpp"

#include "world/item/Components.hpp"

#include <fstream>
#include <iostream>

namespace {
	constexpr int Y = LevelFixture::SURFACE + 1;

	struct CraftingFixture {
		LevelFixture			f;
		std::shared_ptr<Player> player;
		Menu*					menu = nullptr;

		CraftingFixture() : player(std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server)) {
			player->setPosition(0.5, Y, 1.5);
			f.set(0, Y, 0, "minecraft:crafting_table");
			int state = f.at(0, Y, 0);
			f.level->behavior(state).useWithoutItem(*f.level, {0, Y, 0}, state, *player);
			menu = &Menus::current(*player, *f.level);
		}
		// Grid cell 0-8 (row by row)
		void put(int cell, const char* item, int count = 1) { menu->slots()[1 + cell].set(ItemStack(f.item(item), count)); }
		const ItemStack& result() { return menu->slots()[0].item(); }
		const ItemStack& cell(int index) { return menu->slots()[1 + index].item(); }
		int				 inInventory(const char* item) {
			int total = 0;
			for (int slot = 0; slot < PlayerInventory::SIZE; slot++) {
				if (player->inventory().get(slot).item == f.item(item)) total += player->inventory().get(slot).count;
			}
			return total;
		}
	};
} // namespace

TEST(recipes_loaded) {
	LevelFixture f;
	CHECK(f.level->recipes().size() > 1400);
	CHECK(f.level->recipes().byId("minecraft:acacia_boat") != nullptr);
}

// Shaped: anywhere in the grid, mirrored too; tags as ingredients
TEST(recipes_shaped) {
	CraftingFixture c;
	c.put(4, "minecraft:birch_planks");
	c.put(5, "minecraft:oak_planks");
	c.put(7, "minecraft:oak_planks");
	CHECK(c.result().isEmpty());
	c.put(8, "minecraft:spruce_planks");
	CHECK_EQ(c.result().item, c.f.item("minecraft:crafting_table")); // Bottom right corner
	// Stairs, mirrored
	for (int i = 0; i < 9; i++) c.menu->slots()[1 + i].set(ItemStack());
	for (int cell : {2, 4, 5, 6, 7, 8}) c.put(cell, "minecraft:oak_planks");
	CHECK_EQ(c.result().item, c.f.item("minecraft:oak_stairs"));
	CHECK_EQ(c.result().count, 4);
	c.put(0, "minecraft:oak_planks"); // One too many
	CHECK(c.result().isEmpty());
}

// Shapeless: any cells; taking the result uses one of each
TEST(recipes_shapeless_take) {
	CraftingFixture c;
	c.put(8, "minecraft:flint", 2);
	c.put(0, "minecraft:iron_ingot", 3);
	CHECK_EQ(c.result().item, c.f.item("minecraft:flint_and_steel"));
	c.menu->clicked(0, 0, ClickType::Pickup);
	CHECK_EQ(c.menu->carried().item, c.f.item("minecraft:flint_and_steel"));
	CHECK_EQ(c.cell(8).count, 1);
	CHECK_EQ(c.cell(0).count, 2);
	CHECK_EQ(c.result().item, c.f.item("minecraft:flint_and_steel")); // Again
	// The cursor can't take another one (different item on it... same item but the stack is full: flint and steel
	// doesn't stack)
	c.menu->clicked(0, 0, ClickType::Pickup);
	CHECK_EQ(c.menu->carried().count, 1);
	CHECK_EQ(c.cell(8).count, 1);
}

// The milk buckets of a cake leave their buckets in the grid
TEST(recipes_remainders) {
	CraftingFixture c;
	for (int cell : {0, 1, 2}) c.put(cell, "minecraft:milk_bucket");
	c.put(3, "minecraft:sugar");
	c.put(4, "minecraft:egg");
	c.put(5, "minecraft:sugar");
	for (int cell : {6, 7, 8}) c.put(cell, "minecraft:wheat");
	CHECK_EQ(c.result().item, c.f.item("minecraft:cake"));
	c.menu->clicked(0, 0, ClickType::Pickup);
	CHECK_EQ(c.menu->carried().item, c.f.item("minecraft:cake"));
	for (int cell : {0, 1, 2}) CHECK_EQ(c.cell(cell).item, c.f.item("minecraft:bucket"));
	for (int cell = 3; cell < 9; cell++) CHECK(c.cell(cell).isEmpty());
	CHECK(c.result().isEmpty());
}

// Shift-click on the result crafts as long as it can
TEST(recipes_shift_click_result) {
	CraftingFixture c;
	c.put(4, "minecraft:oak_log", 3);
	CHECK_EQ(c.result().count, 4);
	c.menu->clicked(0, 0, ClickType::QuickMove);
	CHECK_EQ(c.inInventory("minecraft:oak_planks"), 12);
	CHECK(c.cell(4).isEmpty());
	CHECK(c.result().isEmpty());
}

// Transmute: a dyed shulker box keeps its components
TEST(recipes_transmute) {
	CraftingFixture c;
	c.put(0, "minecraft:shulker_box");
	c.put(4, "minecraft:red_dye");
	CHECK_EQ(c.result().item, c.f.item("minecraft:red_shulker_box"));
	c.menu->slots()[1].set(ItemStack(c.f.item("minecraft:red_shulker_box"), 1)); // Already red: nothing
	CHECK(c.result().isEmpty());
}

// The inventory's 2x2 grid
TEST(recipes_inventory_grid) {
	LevelFixture f;
	auto  player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	Menu& menu	 = Menus::inventory(*player, *f.level);
	for (int slot = 1; slot <= 4; slot++) menu.slots()[slot].set(ItemStack(f.item("minecraft:oak_planks"), 1));
	CHECK_EQ(menu.slots()[0].item().item, f.item("minecraft:crafting_table"));
	menu.clicked(0, 0, ClickType::QuickMove);
	CHECK_EQ(player->inventory().get(PlayerInventory::HOTBAR + 8).item, f.item("minecraft:crafting_table")); // Hotbar, from the end
	for (int slot = 0; slot <= 4; slot++) CHECK(menu.slots()[slot].item().isEmpty());
}

// The recipes/ folder: replaces, adds and removes recipes
TEST(recipes_overrides) {
	LevelFixture		  f;
	std::filesystem::path directory = f.directory / "recipes";
	std::filesystem::create_directories(directory / "mypack");
	std::ofstream(directory / "acacia_boat.json") << R"({"remove": true})";
	std::ofstream(directory / "mypack" / "dirt_diamond.json")
			<< R"({"type": "minecraft:crafting_shaped", "pattern": ["DD", "DD"], "key": {"D": "minecraft:dirt"}, "result": {"id": "minecraft:diamond"}})";
	std::ofstream(directory / "broken.json") << R"({"type": "minecraft:crafting_shaped"})";
	RecipeManager recipes;
	recipes.load(f.data.getDirectory() / "recipes.json", directory, f.data);
	CHECK(recipes.byId("minecraft:acacia_boat") == nullptr);
	CHECK(recipes.byId("minecraft:broken") == nullptr);
	const Recipe* diamond = recipes.byId("mypack:dirt_diamond");
	CHECK(diamond != nullptr);
	CraftingInput::Positioned input = CraftingInput::ofPositioned(
			3, 3, {ItemStack(), ItemStack(), ItemStack(), ItemStack(), ItemStack(f.item("minecraft:dirt"), 1), ItemStack(f.item("minecraft:dirt"), 1),
				   ItemStack(), ItemStack(f.item("minecraft:dirt"), 1), ItemStack(f.item("minecraft:dirt"), 1)});
	CHECK_EQ(input.left, 1);
	CHECK_EQ(input.top, 1);
	CHECK(recipes.getRecipeFor(RecipeType::Crafting, input.input) == diamond);
}

namespace {
	// Reads the recipe book's entries back, as the client would (RecipeDisplayEntry.STREAM_CODEC)
	struct EntryReader {
		const std::vector<uint8_t>& data;
		const GameData&				gameData;
		size_t						pos = 0;

		int varint() {
			int value = 0, shift = 0;
			while (true) {
				uint8_t byte = data.at(pos++);
				value |= (byte & 0x7F) << shift;
				if (!(byte & 0x80)) return value;
				shift += 7;
			}
		}
		std::string string() {
			int			length = varint();
			std::string value(data.begin() + static_cast<long>(pos), data.begin() + static_cast<long>(pos) + length);
			pos += static_cast<size_t>(length);
			return value;
		}
		bool registry(const char* name, int id) { return !gameData.getStaticName(name, id).empty(); }
		bool slot() {
			std::string type = gameData.getStaticName("minecraft:slot_display", varint());
			if (type == "minecraft:empty" || type == "minecraft:any_fuel") return true;
			if (type == "minecraft:item") return registry("minecraft:item", varint());
			if (type == "minecraft:item_stack") return Components::readStack(data, pos, gameData).has_value();
			if (type == "minecraft:tag") return !string().empty();
			if (type == "minecraft:with_remainder") return slot() && slot();
			if (type == "minecraft:composite") {
				for (int i = varint(); i > 0; i--) {
					if (!slot()) return false;
				}
				return true;
			}
			return false;
		}
		bool display() {
			std::string type = gameData.getStaticName("minecraft:recipe_display", varint());
			int			slots;
			if (type == "minecraft:crafting_shaped") {
				int width = varint(), height = varint();
				slots	  = varint();
				if (slots != width * height) return false;
				slots += 2;
			} else if (type == "minecraft:crafting_shapeless") {
				slots = varint() + 2;
			} else if (type == "minecraft:furnace") {
				slots = 4;
			} else if (type == "minecraft:stonecutter") {
				slots = 3;
			} else {
				return false;
			}
			for (int i = 0; i < slots; i++) {
				if (!slot()) return false;
			}
			if (type == "minecraft:furnace") {
				varint();
				pos += 4; // Experience
			}
			return true;
		}
		bool entry(int expectedId) {
			if (varint() != expectedId || !display()) return false;
			varint(); // Group + 1
			if (!registry("minecraft:recipe_book_category", varint())) return false;
			if (data.at(pos++) == 1) {
				for (int i = varint(); i > 0; i--) {
					int count = varint();
					if (count == 0) {
						string();
					} else {
						for (int j = 1; j < count; j++) {
							if (!registry("minecraft:item", varint())) return false;
						}
					}
				}
			}
			return pos == data.size();
		}
	};
} // namespace

// Every recipe with a display is in the book (not the special ones), each entry reads back whole
TEST(recipe_book_entries) {
	LevelFixture									f;
	const std::vector<RecipeManager::DisplayInfo>& displays = f.level->recipes().displays();
	CHECK(displays.size() > 1400);
	for (size_t i = 0; i < displays.size(); i++) {
		EntryReader reader{displays[i].entry, f.data};
		bool		ok = false;
		try {
			ok = reader.entry(static_cast<int>(i));
		} catch (const std::exception&) {
		}
		if (!ok) std::cout << "    bad entry " << displays[i].recipe->id << "\n";
		CHECK(ok);
		if (!ok) break;
	}
}

namespace {
	int displayOf(LevelFixture& f, const char* id) {
		const std::vector<RecipeManager::DisplayInfo>& displays = f.level->recipes().displays();
		for (size_t i = 0; i < displays.size(); i++) {
			if (displays[i].recipe->id == id) return static_cast<int>(i);
		}
		return -1;
	}
} // namespace

// A click on a recipe of the book: once, or as many as possible; missing items: a ghost, the grid emptied
TEST(recipe_book_places) {
	CraftingFixture c;
	auto*			menu = dynamic_cast<CraftingGridMenu*>(c.menu);
	CHECK(menu != nullptr);
	c.player->inventory().set(PlayerInventory::HOTBAR, ItemStack(c.f.item("minecraft:oak_planks"), 10));
	const Recipe& table = *c.f.level->recipes().byId("minecraft:crafting_table");
	CHECK(!menu->placeRecipe(table, false));
	for (int cell : {0, 1, 3, 4}) CHECK_EQ(c.cell(cell).count, 1);
	CHECK_EQ(c.result().item, c.f.item("minecraft:crafting_table"));
	CHECK(!menu->placeRecipe(table, false)); // Once more
	for (int cell : {0, 1, 3, 4}) CHECK_EQ(c.cell(cell).count, 2);
	CHECK(!menu->placeRecipe(table, true)); // All: 10 planks, 2 each
	for (int cell : {0, 1, 3, 4}) CHECK_EQ(c.cell(cell).count, 2);
	CHECK_EQ(c.inInventory("minecraft:oak_planks"), 2);

	// Another recipe: the grid goes back into the inventory first (8 planks there, 2 in the inventory: 10 for 8)
	const Recipe& chest = *c.f.level->recipes().byId("minecraft:chest");
	CHECK(!menu->placeRecipe(chest, false));
	for (int cell = 0; cell < 9; cell++) CHECK(cell == 4 ? c.cell(cell).isEmpty() : c.cell(cell).count == 1);
	CHECK_EQ(c.inInventory("minecraft:oak_planks"), 2);

	const Recipe& diamondBlock = *c.f.level->recipes().byId("minecraft:diamond_block");
	CHECK(menu->placeRecipe(diamondBlock, false)); // Nothing for it: a ghost, the grid back into the inventory
	for (int cell = 0; cell < 9; cell++) CHECK(c.cell(cell).isEmpty());
	CHECK_EQ(c.inInventory("minecraft:oak_planks"), 10);
	CHECK(displayOf(c.f, "minecraft:chest") >= 0);
}

// The inventory's grid: a 2x2 recipe from the book
TEST(recipe_book_inventory_grid) {
	LevelFixture f;
	auto		 player = std::make_shared<Player>("Bob", PlayerState::Play, -1, f.server);
	player->inventory().set(PlayerInventory::HOTBAR, ItemStack(f.item("minecraft:birch_log"), 3));
	auto& menu = dynamic_cast<CraftingGridMenu&>(Menus::inventory(*player, *f.level));
	CHECK(!menu.placeRecipe(*f.level->recipes().byId("minecraft:birch_planks"), true));
	CHECK_EQ(menu.slots()[1].item().count, 3);
	CHECK_EQ(menu.slots()[0].item().item, f.item("minecraft:birch_planks"));
}
