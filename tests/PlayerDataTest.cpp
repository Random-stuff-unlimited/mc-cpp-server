#include "Test.hpp"
#include "data/GameData.hpp"
#include "lib/compression.hpp"
#include "lib/nbtParser.hpp"
#include "lib/nbtWriter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/PlayerDataStorage.hpp"
#include "world/item/Components.hpp"
#include "world/item/ItemNbt.hpp"

#include <filesystem>
#include <fstream>
#include <memory>

namespace {
	struct PlayerDataFixture {
		GameData			  data;
		Server				  server;
		std::filesystem::path directory;

		PlayerDataFixture() {
			data.load("resources/gamedata");
			directory = std::filesystem::temp_directory_path() / ("mc-cpp-server-playerdata-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
			std::filesystem::remove_all(directory);
		}
		~PlayerDataFixture() { std::filesystem::remove_all(directory); }

		int								item(const std::string& name) const { return data.getStaticId("minecraft:item", name); }
		std::shared_ptr<Player>			player() { return std::make_shared<Player>("Bob", PlayerState::Play, -1, server); }
		int								component(const std::string& name) const { return Components::typeId(data, name); }
	};

	std::vector<uint8_t> varint(int value) {
		std::vector<uint8_t> out;
		uint32_t			 v = static_cast<uint32_t>(value);
		while (v >= 0x80) {
			out.push_back(static_cast<uint8_t>(v | 0x80));
			v >>= 7;
		}
		out.push_back(static_cast<uint8_t>(v));
		return out;
	}
	std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts) {
		std::vector<uint8_t> out;
		for (const auto& part : parts) out.insert(out.end(), part.begin(), part.end());
		return out;
	}
	nbt::Tag compoundTag(nbt::TagCompound compound) { return nbt::Tag(std::make_shared<nbt::TagCompound>(std::move(compound))); }
	nbt::Tag listTag(nbt::TagList list) { return nbt::Tag(std::make_shared<nbt::TagList>(std::move(list))); }
	bool	 sameStack(const ItemStack& a, const ItemStack& b) { return a.item == b.item && a.count == b.count && a.components == b.components; }
} // namespace

// The NBT writer's output reads back the same
TEST(playerdata_nbt_round_trip) {
	nbt::TagCompound root;
	root["b"]	  = nbt::TagByte(-3);
	root["s"]	  = nbt::TagShort(1234);
	root["i"]	  = nbt::TagInt(-99999);
	root["l"]	  = nbt::TagLong(1LL << 40);
	root["f"]	  = nbt::TagFloat(1.5F);
	root["d"]	  = nbt::TagDouble(-2.25);
	root["str"]	  = nbt::TagString("hello");
	root["ints"]  = nbt::TagIntArray{1, -2, 3};
	root["empty"] = listTag({});
	root["list"]  = listTag(nbt::TagList(std::vector<double>{1.0, 2.0}));
	nbt::TagCompound child;
	child["x"]	   = nbt::TagInt(7);
	root["child"]  = compoundTag(child);
	nbt::NBT back  = nbt::Parser().parse(nbt::Writer::write(root));
	const auto& r  = back.getRoot();
	CHECK_EQ(r.at("b").get<nbt::TagByte>(), -3);
	CHECK_EQ(r.at("s").get<nbt::TagShort>(), 1234);
	CHECK_EQ(r.at("i").get<nbt::TagInt>(), -99999);
	CHECK_EQ(r.at("l").get<nbt::TagLong>(), 1LL << 40);
	CHECK(r.at("f").get<nbt::TagFloat>() == 1.5F);
	CHECK(r.at("d").get<nbt::TagDouble>() == -2.25);
	CHECK(r.at("str").get<nbt::TagString>() == std::string("hello"));
	CHECK(r.at("ints").get<nbt::TagIntArray>() == (nbt::TagIntArray{1, -2, 3}));
	CHECK_EQ(r.at("empty").get<std::shared_ptr<nbt::TagList>>()->size(), 0u);
	CHECK(r.at("list").get<std::shared_ptr<nbt::TagList>>()->data[1].get<nbt::TagDouble>() == 2.0);
	CHECK_EQ(r.at("child").get<std::shared_ptr<nbt::TagCompound>>()->at("x").get<nbt::TagInt>(), 7);
}

// Components the server knows become vanilla NBT and come back identical
TEST(playerdata_item_components) {
	PlayerDataFixture f;
	int				  sharpness = f.data.getSyncedId("minecraft:enchantment", "minecraft:sharpness");
	CHECK(sharpness >= 0);
	ComponentPatch patch;
	patch.set(f.component("minecraft:damage"), varint(12));
	patch.set(f.component("minecraft:enchantments"), concat({varint(1), varint(sharpness), varint(3)}));
	patch.set(f.component("minecraft:rarity"), varint(2));
	patch.removed.push_back(f.component("minecraft:attribute_modifiers"));
	ItemStack sword(f.item("minecraft:diamond_sword"), 1);
	sword.components = patch.encode();

	nbt::TagCompound tag = ItemNbt::save(sword, f.data);
	CHECK(tag.at("id").get<nbt::TagString>() == std::string("minecraft:diamond_sword"));
	CHECK_EQ(tag.at("count").get<nbt::TagInt>(), 1);
	CHECK(!tag.contains(ItemNbt::RAW_COMPONENTS)); // Fully converted
	const auto& components = *tag.at("components").get<std::shared_ptr<nbt::TagCompound>>();
	CHECK_EQ(components.at("minecraft:damage").get<nbt::TagInt>(), 12);
	CHECK(components.at("minecraft:rarity").get<nbt::TagString>() == std::string("rare"));
	CHECK_EQ(components.at("minecraft:enchantments").get<std::shared_ptr<nbt::TagCompound>>()->at("minecraft:sharpness").get<nbt::TagInt>(), 3);
	CHECK(components.contains("!minecraft:attribute_modifiers"));

	// Read back from vanilla NBT only (as if written by another version)
	CHECK(sameStack(ItemNbt::load(tag, f.data, false), sword));

	// A shulker box with items inside (minecraft:container)
	ItemStack box(f.item("minecraft:shulker_box"), 1);
	CHECK(Components::set(box, f.data, "minecraft:container", Components::encodeContainer({ItemStack(), ItemStack(f.item("minecraft:stone"), 5)})));
	CHECK(sameStack(ItemNbt::load(ItemNbt::save(box, f.data), f.data, false), box));
}

// A component the server can't read is kept raw on this server, and dropped (item kept) elsewhere
TEST(playerdata_item_unknown_component) {
	PlayerDataFixture f;
	ItemStack		  bread(f.item("minecraft:bread"), 3);
	bread.components = concat({varint(1), varint(0), varint(f.component("minecraft:food")), {1, 2, 3, 4, 5}});
	nbt::TagCompound tag = ItemNbt::save(bread, f.data);
	CHECK(tag.contains(ItemNbt::RAW_COMPONENTS));
	CHECK(sameStack(ItemNbt::load(tag, f.data, true), bread));
	ItemStack withoutRaw = ItemNbt::load(tag, f.data, false);
	CHECK_EQ(withoutRaw.item, bread.item);
	CHECK_EQ(withoutRaw.count, 3);
	CHECK(withoutRaw.components.empty());
}

// Saved then loaded into a fresh player: the same state
TEST(playerdata_save_load) {
	PlayerDataFixture f;
	auto			  player = f.player();
	player->setUUID(UUID(0x0123456789abcdefULL, 0xfedcba9876543210ULL));
	player->setPosition(12.5, 70, -8.25);
	player->setRotation(90, -30);
	player->setOnGround(true);
	player->setGameMode(GameMode::Creative);
	player->setPreviousGameMode(0);
	player->setSelectedSlot(5);
	player->combat().health		= 13.5F;
	player->foodData().setFoodLevel(17);
	player->foodData().setSaturation(2.5F);
	player->foodData().setExhaustion(1.5F);
	player->foodData().setTickTimer(12);
	player->setAirSupply(123);
	player->combat().hasDeathLocation = true;
	player->combat().deathX = 1, player->combat().deathY = -60, player->combat().deathZ = 3;
	player->inventory().set(PlayerInventory::HOTBAR + 5, ItemStack(f.item("minecraft:oak_log"), 64));
	player->inventory().set(9, ItemStack(f.item("minecraft:cobblestone"), 12));
	player->inventory().set(5, ItemStack(f.item("minecraft:iron_helmet"), 1));	// Head
	player->inventory().set(8, ItemStack(f.item("minecraft:iron_boots"), 1));	// Feet
	player->inventory().set(PlayerInventory::OFFHAND, ItemStack(f.item("minecraft:shield"), 1));
	ItemStack named(f.item("minecraft:diamond"), 2);
	Components::set(named, f.data, "minecraft:damage", varint(4));
	player->inventory().set(PlayerInventory::HOTBAR, named);
	player->enderChest()[0]	 = ItemStack(f.item("minecraft:emerald"), 20);
	player->enderChest()[26] = ItemStack(f.item("minecraft:gold_ingot"), 7);
	player->recipeBookSettings()[0] = true; // Crafting book open
	player->recipeBookSettings()[3] = true; // Furnace filtering

	PlayerDataStorage storage(f.directory);
	storage.save(player->getUUID(), PlayerData::save(*player, f.data, "minecraft:overworld"));
	CHECK(std::filesystem::exists(f.directory / "playerdata" / (player->getUUID().toString() + ".dat")));
	// A second save keeps the previous file as .dat_old
	storage.save(player->getUUID(), PlayerData::save(*player, f.data, "minecraft:overworld"));
	CHECK(std::filesystem::exists(f.directory / "playerdata" / (player->getUUID().toString() + ".dat_old")));

	PlayerDataStorage				reader(f.directory);
	std::optional<nbt::TagCompound> saved = reader.load(player->getUUID());
	CHECK(saved.has_value());
	if (!saved) return;
	CHECK(PlayerData::dimension(*saved) == std::string("minecraft:overworld"));
	auto loaded = f.player();
	PlayerData::load(*loaded, *saved, f.data, GameMode::Survival);
	CHECK(loaded->getX() == 12.5 && loaded->getY() == 70 && loaded->getZ() == -8.25);
	CHECK(loaded->getYaw() == 90 && loaded->getPitch() == -30);
	CHECK(loaded->isOnGround());
	CHECK(loaded->getGameMode() == GameMode::Creative);
	CHECK_EQ(loaded->getPreviousGameMode(), 0);
	CHECK_EQ(loaded->getSelectedSlot(), 5);
	CHECK(loaded->combat().health == 13.5F);
	CHECK_EQ(loaded->foodData().getFoodLevel(), 17);
	CHECK(loaded->foodData().getSaturationLevel() == 2.5F);
	CHECK(loaded->foodData().getExhaustionLevel() == 1.5F);
	CHECK_EQ(loaded->foodData().getTickTimer(), 12);
	CHECK_EQ(loaded->getAirSupply(), 123);
	CHECK(loaded->combat().hasDeathLocation && loaded->combat().deathY == -60);
	for (int slot = 0; slot < PlayerInventory::SIZE; slot++) CHECK(sameStack(loaded->inventory().get(slot), player->inventory().get(slot)));
	for (int slot = 0; slot < 27; slot++) CHECK(sameStack(loaded->enderChest()[slot], player->enderChest()[slot]));
	CHECK(loaded->recipeBookSettings() == player->recipeBookSettings());

	// Vanilla's layout: armor and offhand in "equipment", the rest in "Inventory" with Slot bytes
	const auto& equipment = *saved->at("equipment").get<std::shared_ptr<nbt::TagCompound>>();
	CHECK(equipment.contains("head") && equipment.contains("feet") && equipment.contains("offhand"));
	CHECK_EQ(saved->at("Inventory").get<std::shared_ptr<nbt::TagList>>()->size(), 3u);
	CHECK_EQ(saved->at("playerGameType").get<nbt::TagInt>(), 1);
}

// A player that never played: no data, and the storage doesn't invent any
TEST(playerdata_missing) {
	PlayerDataFixture f;
	PlayerDataStorage storage(f.directory);
	CHECK(!storage.load(UUID(1, 2)).has_value());
	// Missing keys take vanilla's defaults
	auto player = f.player();
	player->setSelectedSlot(4);
	PlayerData::load(*player, nbt::TagCompound(), f.data, GameMode::Adventure);
	CHECK(player->getGameMode() == GameMode::Adventure);
	CHECK_EQ(player->getSelectedSlot(), 0);
	CHECK(player->combat().health == 20);
	CHECK_EQ(player->foodData().getFoodLevel(), 20);
	CHECK(player->foodData().getSaturationLevel() == 5);
	CHECK_EQ(player->getAirSupply(), 300);
	CHECK_EQ(player->getPreviousGameMode(), -1);
}

// A file shaped like vanilla's (gzipped, pre-1.21.5 armor slots too), including components the server can't convert
TEST(playerdata_vanilla_file) {
	PlayerDataFixture f;
	auto			  item = [&](const std::string& id, int count, int slot) {
		 nbt::TagCompound entry;
		 entry["id"]	= nbt::TagString(id);
		 entry["count"] = nbt::TagInt(count);
		 if (slot >= 0) entry["Slot"] = nbt::TagByte(static_cast<int8_t>(slot));
		 return entry;
	};
	nbt::TagCompound root;
	root["DataVersion"] = nbt::TagInt(4000); // Another version: no raw components trusted
	root["Pos"]			= listTag(nbt::TagList(std::vector<double>{100.5, 64, -20.5}));
	root["Rotation"]	= listTag(nbt::TagList(std::vector<float>{45, 10}));
	root["Dimension"]	= nbt::TagString("minecraft:overworld");
	root["Health"]		= nbt::TagFloat(7);
	root["foodLevel"]	= nbt::TagInt(3);
	root["playerGameType"] = nbt::TagInt(2);
	root["SelectedItemSlot"] = nbt::TagInt(8);
	nbt::TagList inventory;
	inventory.push_back(compoundTag(item("minecraft:torch", 32, 8)));
	nbt::TagCompound pick = item("minecraft:diamond_pickaxe", 1, 0);
	nbt::TagCompound components;
	components["minecraft:damage"] = nbt::TagInt(100);
	nbt::TagCompound food; // Not converted by the server: dropped, the item stays
	food["nutrition"]			 = nbt::TagInt(4);
	components["minecraft:food"] = compoundTag(food);
	components["minecraft:custom_name"] = nbt::TagString("Pick");
	pick["components"]			 = compoundTag(components);
	inventory.push_back(compoundTag(pick));
	inventory.push_back(compoundTag(item("minecraft:not_an_item", 1, 1))); // Removed item: lost
	inventory.push_back(compoundTag(item("minecraft:diamond_chestplate", 1, 102)));
	root["Inventory"] = listTag(inventory);
	nbt::TagCompound equipment;
	equipment["offhand"] = compoundTag(item("minecraft:totem_of_undying", 1, -1));
	root["equipment"]	 = compoundTag(equipment);
	nbt::TagList ender;
	ender.push_back(compoundTag(item("minecraft:ender_pearl", 16, 13)));
	root["EnderItems"] = listTag(ender);
	nbt::TagCompound book;
	book["isFurnaceGuiOpen"] = nbt::TagByte(1);
	root["recipeBook"]		 = compoundTag(book);

	UUID uuid(42, 43);
	std::filesystem::create_directories(f.directory / "playerdata");
	std::vector<uint8_t> raw  = nbt::Writer::write(root);
	std::vector<uint8_t> gzip = compression::gzipCompress(raw.data(), raw.size(), 6);
	{
		std::ofstream out(f.directory / "playerdata" / (uuid.toString() + ".dat"), std::ios::binary);
		out.write(reinterpret_cast<const char*>(gzip.data()), static_cast<std::streamsize>(gzip.size()));
	}

	PlayerDataStorage				storage(f.directory);
	std::optional<nbt::TagCompound> saved = storage.load(uuid);
	CHECK(saved.has_value());
	if (!saved) return;
	auto player = f.player();
	PlayerData::load(*player, *saved, f.data, GameMode::Survival);
	CHECK(player->getX() == 100.5 && player->getY() == 64 && player->getZ() == -20.5);
	CHECK(player->getYaw() == 45 && player->getPitch() == 10);
	CHECK(player->combat().health == 7);
	CHECK_EQ(player->foodData().getFoodLevel(), 3);
	CHECK(player->getGameMode() == GameMode::Adventure);
	CHECK_EQ(player->getSelectedSlot(), 8);
	CHECK_EQ(player->inventory().get(PlayerInventory::HOTBAR + 8).item, f.item("minecraft:torch"));
	CHECK_EQ(player->inventory().get(PlayerInventory::HOTBAR + 8).count, 32);
	const ItemStack& pickaxe = player->inventory().get(PlayerInventory::HOTBAR);
	CHECK_EQ(pickaxe.item, f.item("minecraft:diamond_pickaxe"));
	std::optional<std::vector<uint8_t>> damage = Components::get(pickaxe, f.data, "minecraft:damage");
	CHECK(damage && *damage == varint(100));
	CHECK(Components::get(pickaxe, f.data, "minecraft:custom_name").has_value());
	CHECK(!Components::get(pickaxe, f.data, "minecraft:food").has_value());
	CHECK(player->inventory().get(PlayerInventory::HOTBAR + 1).isEmpty());
	CHECK_EQ(player->inventory().get(6).item, f.item("minecraft:diamond_chestplate")); // Slot 102: chest
	CHECK_EQ(player->inventory().get(PlayerInventory::OFFHAND).item, f.item("minecraft:totem_of_undying"));
	CHECK_EQ(player->enderChest()[13].count, 16);
	CHECK(player->recipeBookSettings()[2] && !player->recipeBookSettings()[0]);

	// Saved again with this version's DataVersion; the keys the server doesn't simulate stay as they were
	nbt::TagCompound again = PlayerData::save(*player, f.data, "minecraft:overworld");
	CHECK_EQ(again.at("DataVersion").get<nbt::TagInt>(), f.data.getDataVersion());
	CHECK_EQ(again.at("foodLevel").get<nbt::TagInt>(), 3);
	CHECK(again.contains("recipeBook") && again.contains("EnderItems"));
}

// An unreadable file falls back on .dat_old
TEST(playerdata_corrupted_falls_back) {
	PlayerDataFixture f;
	UUID			  uuid(7, 8);
	PlayerDataStorage storage(f.directory);
	auto			  player = f.player();
	player->setUUID(uuid);
	player->setPosition(1, 2, 3);
	storage.save(uuid, PlayerData::save(*player, f.data, "minecraft:overworld"));
	storage.save(uuid, PlayerData::save(*player, f.data, "minecraft:overworld"));
	{
		std::ofstream out(f.directory / "playerdata" / (uuid.toString() + ".dat"), std::ios::binary | std::ios::trunc);
		out << "garbage";
	}
	PlayerDataStorage				reader(f.directory);
	std::optional<nbt::TagCompound> saved = reader.load(uuid);
	CHECK(saved.has_value());
	if (saved) {
		auto loaded = f.player();
		PlayerData::load(*loaded, *saved, f.data, GameMode::Survival);
		CHECK(loaded->getY() == 2);
	}
}
