#include "world/item/FuelValues.hpp"

#include "data/GameData.hpp"

#include <string>

FuelValues::FuelValues(const GameData& gameData, int standard) {
	int items = 0;
	while (!gameData.getStaticName("minecraft:item", items).empty()) items++;
	_byItem.assign(static_cast<size_t>(items), 0);

	// FuelValues.Builder: add(item or #tag, ticks), a later value replacing an earlier one; remove(#tag)
	auto add = [&](const std::string& name, int ticks) {
		if (name[0] == '#') {
			for (int id = 0; id < items; id++) {
				if (gameData.isInTag("minecraft:item", name.substr(1), id)) _byItem[id] = ticks;
			}
		} else if (int id = gameData.getStaticId("minecraft:item", name); id > 0) {
			_byItem[id] = ticks;
		}
	};
	auto remove = [&](const std::string& tag) {
		for (int id = 0; id < items; id++) {
			if (gameData.isInTag("minecraft:item", tag, id)) _byItem[id] = 0;
		}
	};

	const int s = standard;
	add("minecraft:lava_bucket", s * 100);
	add("minecraft:coal_block", s * 8 * 10);
	add("minecraft:blaze_rod", s * 12);
	add("minecraft:coal", s * 8);
	add("minecraft:charcoal", s * 8);
	add("#minecraft:logs", s * 3 / 2);
	add("#minecraft:bamboo_blocks", s * 3 / 2);
	add("#minecraft:planks", s * 3 / 2);
	add("minecraft:bamboo_mosaic", s * 3 / 2);
	add("#minecraft:wooden_stairs", s * 3 / 2);
	add("minecraft:bamboo_mosaic_stairs", s * 3 / 2);
	add("#minecraft:wooden_slabs", s * 3 / 4);
	add("minecraft:bamboo_mosaic_slab", s * 3 / 4);
	add("#minecraft:wooden_trapdoors", s * 3 / 2);
	add("#minecraft:wooden_pressure_plates", s * 3 / 2);
	add("#minecraft:wooden_shelves", s * 3 / 2);
	add("#minecraft:wooden_fences", s * 3 / 2);
	add("#minecraft:fence_gates", s * 3 / 2);
	add("minecraft:note_block", s * 3 / 2);
	add("minecraft:bookshelf", s * 3 / 2);
	add("minecraft:chiseled_bookshelf", s * 3 / 2);
	add("minecraft:lectern", s * 3 / 2);
	add("minecraft:jukebox", s * 3 / 2);
	add("minecraft:chest", s * 3 / 2);
	add("minecraft:trapped_chest", s * 3 / 2);
	add("minecraft:crafting_table", s * 3 / 2);
	add("minecraft:daylight_detector", s * 3 / 2);
	add("#minecraft:banners", s * 3 / 2);
	add("minecraft:bow", s * 3 / 2);
	add("minecraft:fishing_rod", s * 3 / 2);
	add("minecraft:ladder", s * 3 / 2);
	add("#minecraft:signs", s);
	add("#minecraft:hanging_signs", s * 4);
	add("minecraft:wooden_shovel", s);
	add("minecraft:wooden_sword", s);
	add("minecraft:wooden_hoe", s);
	add("minecraft:wooden_axe", s);
	add("minecraft:wooden_pickaxe", s);
	add("#minecraft:wooden_doors", s);
	add("#minecraft:boats", s * 6);
	add("#minecraft:wool", s / 2);
	add("#minecraft:wooden_buttons", s / 2);
	add("minecraft:stick", s / 2);
	add("#minecraft:saplings", s / 2);
	add("minecraft:bowl", s / 2);
	add("#minecraft:wool_carpets", 1 + s / 3);
	add("minecraft:dried_kelp_block", 1 + s * 20);
	add("minecraft:crossbow", s * 3 / 2);
	add("minecraft:bamboo", s / 4);
	add("minecraft:dead_bush", s / 2);
	add("minecraft:short_dry_grass", s / 2);
	add("minecraft:tall_dry_grass", s / 2);
	add("minecraft:scaffolding", s / 4);
	add("minecraft:loom", s * 3 / 2);
	add("minecraft:barrel", s * 3 / 2);
	add("minecraft:cartography_table", s * 3 / 2);
	add("minecraft:fletching_table", s * 3 / 2);
	add("minecraft:smithing_table", s * 3 / 2);
	add("minecraft:composter", s * 3 / 2);
	add("minecraft:azalea", s / 2);
	add("minecraft:flowering_azalea", s / 2);
	add("minecraft:mangrove_roots", s * 3 / 2);
	add("minecraft:leaf_litter", s / 2);
	remove("minecraft:non_flammable_wood");
}
