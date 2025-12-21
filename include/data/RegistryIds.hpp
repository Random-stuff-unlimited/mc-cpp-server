#ifndef REGISTRY_IDS_HPP
#define REGISTRY_IDS_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class RegistryIds {
  public:
	static std::map<std::string, uint32_t> getActivity() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:admire_item", 12}, {"minecraft:avoid", 13}, {"minecraft:celebrate", 11}, {"minecraft:core", 0},
				{"minecraft:dig", 25}, {"minecraft:emerge", 24}, {"minecraft:fight", 10}, {"minecraft:hide", 9},
				{"minecraft:idle", 1}, {"minecraft:investigate", 22}, {"minecraft:lay_spawn", 20}, {"minecraft:long_jump", 16},
				{"minecraft:meet", 5}, {"minecraft:panic", 6}, {"minecraft:play", 3}, {"minecraft:play_dead", 15},
				{"minecraft:pre_raid", 8}, {"minecraft:raid", 7}, {"minecraft:ram", 17}, {"minecraft:rest", 4},
				{"minecraft:ride", 14}, {"minecraft:roar", 23}, {"minecraft:sniff", 21}, {"minecraft:swim", 19},
				{"minecraft:tongue", 18}, {"minecraft:work", 2}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getAttribute() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:armor", 0}, {"minecraft:armor_toughness", 1}, {"minecraft:attack_damage", 2}, {"minecraft:attack_knockback", 3},
				{"minecraft:attack_speed", 4}, {"minecraft:block_break_speed", 5}, {"minecraft:block_interaction_range", 6}, {"minecraft:burning_time", 7},
				{"minecraft:camera_distance", 8}, {"minecraft:entity_interaction_range", 10}, {"minecraft:explosion_knockback_resistance", 9}, {"minecraft:fall_damage_multiplier", 11},
				{"minecraft:flying_speed", 12}, {"minecraft:follow_range", 13}, {"minecraft:gravity", 14}, {"minecraft:jump_strength", 15},
				{"minecraft:knockback_resistance", 16}, {"minecraft:luck", 17}, {"minecraft:max_absorption", 18}, {"minecraft:max_health", 19},
				{"minecraft:mining_efficiency", 20}, {"minecraft:movement_efficiency", 21}, {"minecraft:movement_speed", 22}, {"minecraft:oxygen_bonus", 23},
				{"minecraft:safe_fall_distance", 24}, {"minecraft:scale", 25}, {"minecraft:sneaking_speed", 26}, {"minecraft:spawn_reinforcements", 27},
				{"minecraft:step_height", 28}, {"minecraft:submerged_mining_speed", 29}, {"minecraft:sweeping_damage_ratio", 30}, {"minecraft:tempt_range", 31},
				{"minecraft:water_movement_efficiency", 32}, {"minecraft:waypoint_receive_range", 34}, {"minecraft:waypoint_transmit_range", 33}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getBlock() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:acacia_button", 445}, {"minecraft:acacia_door", 647}, {"minecraft:acacia_fence", 638}, {"minecraft:acacia_fence_gate", 629},
				{"minecraft:acacia_hanging_sign", 236}, {"minecraft:acacia_leaves", 92}, {"minecraft:acacia_log", 53}, {"minecraft:acacia_planks", 17},
				{"minecraft:acacia_pressure_plate", 264}, {"minecraft:acacia_sapling", 29}, {"minecraft:acacia_shelf", 179}, {"minecraft:acacia_sign", 212},
				{"minecraft:acacia_slab", 601}, {"minecraft:acacia_stairs", 514}, {"minecraft:acacia_trapdoor", 319}, {"minecraft:acacia_wall_hanging_sign", 248},
				{"minecraft:acacia_wall_sign", 226}, {"minecraft:acacia_wood", 75}, {"minecraft:activator_rail", 480}, {"minecraft:air", 0},
				{"minecraft:allium", 161}, {"minecraft:amethyst_block", 976}, {"minecraft:amethyst_cluster", 978}, {"minecraft:ancient_debris", 914},
				{"minecraft:andesite", 6}, {"minecraft:andesite_slab", 818}, {"minecraft:andesite_stairs", 805}, {"minecraft:andesite_wall", 830},
				{"minecraft:anvil", 465}, {"minecraft:attached_melon_stem", 362}, {"minecraft:attached_pumpkin_stem", 361}, {"minecraft:azalea", 1108},
				{"minecraft:azalea_leaves", 97}, {"minecraft:azure_bluet", 162}, {"minecraft:bamboo", 790}, {"minecraft:bamboo_block", 60},
				{"minecraft:bamboo_button", 450}, {"minecraft:bamboo_door", 652}, {"minecraft:bamboo_fence", 643}, {"minecraft:bamboo_fence_gate", 634},
				{"minecraft:bamboo_hanging_sign", 244}, {"minecraft:bamboo_mosaic", 24}, {"minecraft:bamboo_mosaic_slab", 607}, {"minecraft:bamboo_mosaic_stairs", 520},
				{"minecraft:bamboo_planks", 23}, {"minecraft:bamboo_pressure_plate", 269}, {"minecraft:bamboo_sapling", 789}, {"minecraft:bamboo_shelf", 180},
				{"minecraft:bamboo_sign", 218}, {"minecraft:bamboo_slab", 606}, {"minecraft:bamboo_stairs", 519}, {"minecraft:bamboo_trapdoor", 324},
				{"minecraft:bamboo_wall_hanging_sign", 256}, {"minecraft:bamboo_wall_sign", 232}, {"minecraft:barrel", 837}, {"minecraft:barrier", 522},
				{"minecraft:basalt", 287}, {"minecraft:beacon", 407}, {"minecraft:bedrock", 34}, {"minecraft:bee_nest", 909},
				{"minecraft:beehive", 910}, {"minecraft:beetroots", 663}, {"minecraft:bell", 846}, {"minecraft:big_dripleaf", 1115},
				{"minecraft:big_dripleaf_stem", 1116}, {"minecraft:birch_button", 443}, {"minecraft:birch_door", 645}, {"minecraft:birch_fence", 636},
				{"minecraft:birch_fence_gate", 627}, {"minecraft:birch_hanging_sign", 235}, {"minecraft:birch_leaves", 90}, {"minecraft:birch_log", 51},
				{"minecraft:birch_planks", 15}, {"minecraft:birch_pressure_plate", 262}, {"minecraft:birch_sapling", 27}, {"minecraft:birch_shelf", 181},
				{"minecraft:birch_sign", 211}, {"minecraft:birch_slab", 599}, {"minecraft:birch_stairs", 404}, {"minecraft:birch_trapdoor", 317},
				{"minecraft:birch_wall_hanging_sign", 247}, {"minecraft:birch_wall_sign", 225}, {"minecraft:birch_wood", 73}, {"minecraft:black_banner", 576},
				{"minecraft:black_bed", 125}, {"minecraft:black_candle", 958}, {"minecraft:black_candle_cake", 975}, {"minecraft:black_carpet", 551},
				{"minecraft:black_concrete", 723}, {"minecraft:black_concrete_powder", 739}, {"minecraft:black_glazed_terracotta", 707}, {"minecraft:black_shulker_box", 691},
				{"minecraft:black_stained_glass", 314}, {"minecraft:black_stained_glass_pane", 513}, {"minecraft:black_terracotta", 497}, {"minecraft:black_wall_banner", 592},
				{"minecraft:black_wool", 155}, {"minecraft:blackstone", 922}, {"minecraft:blackstone_slab", 925}, {"minecraft:blackstone_stairs", 923},
				{"minecraft:blackstone_wall", 924}, {"minecraft:blast_furnace", 839}, {"minecraft:blue_banner", 572}, {"minecraft:blue_bed", 121},
				{"minecraft:blue_candle", 954}, {"minecraft:blue_candle_cake", 971}, {"minecraft:blue_carpet", 547}, {"minecraft:blue_concrete", 719},
				{"minecraft:blue_concrete_powder", 735}, {"minecraft:blue_glazed_terracotta", 703}, {"minecraft:blue_ice", 787}, {"minecraft:blue_orchid", 160},
				{"minecraft:blue_shulker_box", 687}, {"minecraft:blue_stained_glass", 310}, {"minecraft:blue_stained_glass_pane", 509}, {"minecraft:blue_terracotta", 493},
				{"minecraft:blue_wall_banner", 588}, {"minecraft:blue_wool", 151}, {"minecraft:bone_block", 672}, {"minecraft:bookshelf", 177},
				{"minecraft:brain_coral", 762}, {"minecraft:brain_coral_block", 752}, {"minecraft:brain_coral_fan", 772}, {"minecraft:brain_coral_wall_fan", 782},
				{"minecraft:brewing_stand", 385}, {"minecraft:brick_slab", 614}, {"minecraft:brick_stairs", 369}, {"minecraft:brick_wall", 822},
				{"minecraft:bricks", 175}, {"minecraft:brown_banner", 573}, {"minecraft:brown_bed", 122}, {"minecraft:brown_candle", 955},
				{"minecraft:brown_candle_cake", 972}, {"minecraft:brown_carpet", 548}, {"minecraft:brown_concrete", 720}, {"minecraft:brown_concrete_powder", 736},
				{"minecraft:brown_glazed_terracotta", 704}, {"minecraft:brown_mushroom", 171}, {"minecraft:brown_mushroom_block", 337}, {"minecraft:brown_shulker_box", 688},
				{"minecraft:brown_stained_glass", 311}, {"minecraft:brown_stained_glass_pane", 510}, {"minecraft:brown_terracotta", 494}, {"minecraft:brown_wall_banner", 589},
				{"minecraft:brown_wool", 152}, {"minecraft:bubble_column", 794}, {"minecraft:bubble_coral", 763}, {"minecraft:bubble_coral_block", 753},
				{"minecraft:bubble_coral_fan", 773}, {"minecraft:bubble_coral_wall_fan", 783}, {"minecraft:budding_amethyst", 977}, {"minecraft:bush", 133},
				{"minecraft:cactus", 278}, {"minecraft:cactus_flower", 279}, {"minecraft:cake", 297}, {"minecraft:calcite", 996},
				{"minecraft:calibrated_sculk_sensor", 1000}, {"minecraft:campfire", 857}, {"minecraft:candle", 942}, {"minecraft:candle_cake", 959},
				{"minecraft:carrots", 439}, {"minecraft:cartography_table", 840}, {"minecraft:carved_pumpkin", 295}, {"minecraft:cauldron", 386},
				{"minecraft:cave_air", 793}, {"minecraft:cave_vines", 1105}, {"minecraft:cave_vines_plant", 1106}, {"minecraft:chain_command_block", 667},
				{"minecraft:cherry_button", 446}, {"minecraft:cherry_door", 648}, {"minecraft:cherry_fence", 639}, {"minecraft:cherry_fence_gate", 630},
				{"minecraft:cherry_hanging_sign", 237}, {"minecraft:cherry_leaves", 93}, {"minecraft:cherry_log", 54}, {"minecraft:cherry_planks", 18},
				{"minecraft:cherry_pressure_plate", 265}, {"minecraft:cherry_sapling", 30}, {"minecraft:cherry_shelf", 182}, {"minecraft:cherry_sign", 213},
				{"minecraft:cherry_slab", 602}, {"minecraft:cherry_stairs", 515}, {"minecraft:cherry_trapdoor", 320}, {"minecraft:cherry_wall_hanging_sign", 249},
				{"minecraft:cherry_wall_sign", 227}, {"minecraft:cherry_wood", 76}, {"minecraft:chest", 200}, {"minecraft:chipped_anvil", 466},
				{"minecraft:chiseled_bookshelf", 178}, {"minecraft:chiseled_copper", 1018}, {"minecraft:chiseled_deepslate", 1138}, {"minecraft:chiseled_nether_bricks", 939},
				{"minecraft:chiseled_polished_blackstone", 929}, {"minecraft:chiseled_quartz_block", 477}, {"minecraft:chiseled_red_sandstone", 594}, {"minecraft:chiseled_resin_bricks", 379},
				{"minecraft:chiseled_sandstone", 107}, {"minecraft:chiseled_stone_bricks", 328}, {"minecraft:chiseled_tuff", 990}, {"minecraft:chiseled_tuff_bricks", 995},
				{"minecraft:chorus_flower", 655}, {"minecraft:chorus_plant", 654}, {"minecraft:clay", 280}, {"minecraft:closed_eyeblossom", 1162},
				{"minecraft:coal_block", 553}, {"minecraft:coal_ore", 46}, {"minecraft:coarse_dirt", 10}, {"minecraft:cobbled_deepslate", 1122},
				{"minecraft:cobbled_deepslate_slab", 1124}, {"minecraft:cobbled_deepslate_stairs", 1123}, {"minecraft:cobbled_deepslate_wall", 1125}, {"minecraft:cobblestone", 12},
				{"minecraft:cobblestone_slab", 613}, {"minecraft:cobblestone_stairs", 222}, {"minecraft:cobblestone_wall", 408}, {"minecraft:cobweb", 129},
				{"minecraft:cocoa", 395}, {"minecraft:command_block", 406}, {"minecraft:comparator", 471}, {"minecraft:composter", 907},
				{"minecraft:conduit", 788}, {"minecraft:copper_bars", 341}, {"minecraft:copper_block", 1005}, {"minecraft:copper_bulb", 1071},
				{"minecraft:copper_chain", 350}, {"minecraft:copper_chest", 1079}, {"minecraft:copper_door", 1047}, {"minecraft:copper_golem_statue", 1087},
				{"minecraft:copper_grate", 1063}, {"minecraft:copper_lantern", 849}, {"minecraft:copper_ore", 1009}, {"minecraft:copper_torch", 291},
				{"minecraft:copper_trapdoor", 1055}, {"minecraft:copper_wall_torch", 292}, {"minecraft:cornflower", 168}, {"minecraft:cracked_deepslate_bricks", 1139},
				{"minecraft:cracked_deepslate_tiles", 1140}, {"minecraft:cracked_nether_bricks", 940}, {"minecraft:cracked_polished_blackstone_bricks", 928}, {"minecraft:cracked_stone_bricks", 327},
				{"minecraft:crafter", 1154}, {"minecraft:crafting_table", 205}, {"minecraft:creaking_heart", 198}, {"minecraft:creeper_head", 459},
				{"minecraft:creeper_wall_head", 460}, {"minecraft:crimson_button", 895}, {"minecraft:crimson_door", 897}, {"minecraft:crimson_fence", 887},
				{"minecraft:crimson_fence_gate", 891}, {"minecraft:crimson_fungus", 874}, {"minecraft:crimson_hanging_sign", 241}, {"minecraft:crimson_hyphae", 871},
				{"minecraft:crimson_nylium", 873}, {"minecraft:crimson_planks", 881}, {"minecraft:crimson_pressure_plate", 885}, {"minecraft:crimson_roots", 880},
				{"minecraft:crimson_shelf", 183}, {"minecraft:crimson_sign", 899}, {"minecraft:crimson_slab", 883}, {"minecraft:crimson_stairs", 893},
				{"minecraft:crimson_stem", 869}, {"minecraft:crimson_trapdoor", 889}, {"minecraft:crimson_wall_hanging_sign", 254}, {"minecraft:crimson_wall_sign", 901},
				{"minecraft:crying_obsidian", 915}, {"minecraft:cut_copper", 1014}, {"minecraft:cut_copper_slab", 1030}, {"minecraft:cut_copper_stairs", 1026},
				{"minecraft:cut_red_sandstone", 595}, {"minecraft:cut_red_sandstone_slab", 620}, {"minecraft:cut_sandstone", 108}, {"minecraft:cut_sandstone_slab", 611},
				{"minecraft:cyan_banner", 570}, {"minecraft:cyan_bed", 119}, {"minecraft:cyan_candle", 952}, {"minecraft:cyan_candle_cake", 969},
				{"minecraft:cyan_carpet", 545}, {"minecraft:cyan_concrete", 717}, {"minecraft:cyan_concrete_powder", 733}, {"minecraft:cyan_glazed_terracotta", 701},
				{"minecraft:cyan_shulker_box", 685}, {"minecraft:cyan_stained_glass", 308}, {"minecraft:cyan_stained_glass_pane", 507}, {"minecraft:cyan_terracotta", 491},
				{"minecraft:cyan_wall_banner", 586}, {"minecraft:cyan_wool", 149}, {"minecraft:damaged_anvil", 467}, {"minecraft:dandelion", 157},
				{"minecraft:dark_oak_button", 447}, {"minecraft:dark_oak_door", 649}, {"minecraft:dark_oak_fence", 640}, {"minecraft:dark_oak_fence_gate", 631},
				{"minecraft:dark_oak_hanging_sign", 239}, {"minecraft:dark_oak_leaves", 94}, {"minecraft:dark_oak_log", 55}, {"minecraft:dark_oak_planks", 19},
				{"minecraft:dark_oak_pressure_plate", 266}, {"minecraft:dark_oak_sapling", 31}, {"minecraft:dark_oak_shelf", 184}, {"minecraft:dark_oak_sign", 215},
				{"minecraft:dark_oak_slab", 603}, {"minecraft:dark_oak_stairs", 516}, {"minecraft:dark_oak_trapdoor", 321}, {"minecraft:dark_oak_wall_hanging_sign", 251},
				{"minecraft:dark_oak_wall_sign", 229}, {"minecraft:dark_oak_wood", 77}, {"minecraft:dark_prismarine", 527}, {"minecraft:dark_prismarine_slab", 533},
				{"minecraft:dark_prismarine_stairs", 530}, {"minecraft:daylight_detector", 472}, {"minecraft:dead_brain_coral", 757}, {"minecraft:dead_brain_coral_block", 747},
				{"minecraft:dead_brain_coral_fan", 767}, {"minecraft:dead_brain_coral_wall_fan", 777}, {"minecraft:dead_bubble_coral", 758}, {"minecraft:dead_bubble_coral_block", 748},
				{"minecraft:dead_bubble_coral_fan", 768}, {"minecraft:dead_bubble_coral_wall_fan", 778}, {"minecraft:dead_bush", 132}, {"minecraft:dead_fire_coral", 759},
				{"minecraft:dead_fire_coral_block", 749}, {"minecraft:dead_fire_coral_fan", 769}, {"minecraft:dead_fire_coral_wall_fan", 779}, {"minecraft:dead_horn_coral", 760},
				{"minecraft:dead_horn_coral_block", 750}, {"minecraft:dead_horn_coral_fan", 770}, {"minecraft:dead_horn_coral_wall_fan", 780}, {"minecraft:dead_tube_coral", 756},
				{"minecraft:dead_tube_coral_block", 746}, {"minecraft:dead_tube_coral_fan", 766}, {"minecraft:dead_tube_coral_wall_fan", 776}, {"minecraft:decorated_pot", 1153},
				{"minecraft:deepslate", 1121}, {"minecraft:deepslate_brick_slab", 1136}, {"minecraft:deepslate_brick_stairs", 1135}, {"minecraft:deepslate_brick_wall", 1137},
				{"minecraft:deepslate_bricks", 1134}, {"minecraft:deepslate_coal_ore", 47}, {"minecraft:deepslate_copper_ore", 1010}, {"minecraft:deepslate_diamond_ore", 203},
				{"minecraft:deepslate_emerald_ore", 398}, {"minecraft:deepslate_gold_ore", 43}, {"minecraft:deepslate_iron_ore", 45}, {"minecraft:deepslate_lapis_ore", 103},
				{"minecraft:deepslate_redstone_ore", 271}, {"minecraft:deepslate_tile_slab", 1132}, {"minecraft:deepslate_tile_stairs", 1131}, {"minecraft:deepslate_tile_wall", 1133},
				{"minecraft:deepslate_tiles", 1130}, {"minecraft:detector_rail", 127}, {"minecraft:diamond_block", 204}, {"minecraft:diamond_ore", 202},
				{"minecraft:diorite", 4}, {"minecraft:diorite_slab", 821}, {"minecraft:diorite_stairs", 808}, {"minecraft:diorite_wall", 834},
				{"minecraft:dirt", 9}, {"minecraft:dirt_path", 664}, {"minecraft:dispenser", 105}, {"minecraft:dragon_egg", 393},
				{"minecraft:dragon_head", 461}, {"minecraft:dragon_wall_head", 462}, {"minecraft:dried_ghast", 745}, {"minecraft:dried_kelp_block", 742},
				{"minecraft:dripstone_block", 1104}, {"minecraft:dropper", 481}, {"minecraft:emerald_block", 402}, {"minecraft:emerald_ore", 397},
				{"minecraft:enchanting_table", 384}, {"minecraft:end_gateway", 665}, {"minecraft:end_portal", 390}, {"minecraft:end_portal_frame", 391},
				{"minecraft:end_rod", 653}, {"minecraft:end_stone", 392}, {"minecraft:end_stone_brick_slab", 814}, {"minecraft:end_stone_brick_stairs", 800},
				{"minecraft:end_stone_brick_wall", 833}, {"minecraft:end_stone_bricks", 659}, {"minecraft:ender_chest", 399}, {"minecraft:exposed_chiseled_copper", 1017},
				{"minecraft:exposed_copper", 1006}, {"minecraft:exposed_copper_bars", 342}, {"minecraft:exposed_copper_bulb", 1072}, {"minecraft:exposed_copper_chain", 351},
				{"minecraft:exposed_copper_chest", 1080}, {"minecraft:exposed_copper_door", 1048}, {"minecraft:exposed_copper_golem_statue", 1088}, {"minecraft:exposed_copper_grate", 1064},
				{"minecraft:exposed_copper_lantern", 850}, {"minecraft:exposed_copper_trapdoor", 1056}, {"minecraft:exposed_cut_copper", 1013}, {"minecraft:exposed_cut_copper_slab", 1029},
				{"minecraft:exposed_cut_copper_stairs", 1025}, {"minecraft:exposed_lightning_rod", 1096}, {"minecraft:farmland", 207}, {"minecraft:fern", 131},
				{"minecraft:fire", 195}, {"minecraft:fire_coral", 764}, {"minecraft:fire_coral_block", 754}, {"minecraft:fire_coral_fan", 774},
				{"minecraft:fire_coral_wall_fan", 784}, {"minecraft:firefly_bush", 1165}, {"minecraft:fletching_table", 841}, {"minecraft:flower_pot", 410},
				{"minecraft:flowering_azalea", 1109}, {"minecraft:flowering_azalea_leaves", 98}, {"minecraft:frogspawn", 1151}, {"minecraft:frosted_ice", 668},
				{"minecraft:furnace", 208}, {"minecraft:gilded_blackstone", 933}, {"minecraft:glass", 101}, {"minecraft:glass_pane", 358},
				{"minecraft:glow_lichen", 366}, {"minecraft:glowstone", 293}, {"minecraft:gold_block", 173}, {"minecraft:gold_ore", 42},
				{"minecraft:granite", 2}, {"minecraft:granite_slab", 817}, {"minecraft:granite_stairs", 804}, {"minecraft:granite_wall", 826},
				{"minecraft:grass_block", 8}, {"minecraft:gravel", 40}, {"minecraft:gray_banner", 568}, {"minecraft:gray_bed", 117},
				{"minecraft:gray_candle", 950}, {"minecraft:gray_candle_cake", 967}, {"minecraft:gray_carpet", 543}, {"minecraft:gray_concrete", 715},
				{"minecraft:gray_concrete_powder", 731}, {"minecraft:gray_glazed_terracotta", 699}, {"minecraft:gray_shulker_box", 683}, {"minecraft:gray_stained_glass", 306},
				{"minecraft:gray_stained_glass_pane", 505}, {"minecraft:gray_terracotta", 489}, {"minecraft:gray_wall_banner", 584}, {"minecraft:gray_wool", 147},
				{"minecraft:green_banner", 574}, {"minecraft:green_bed", 123}, {"minecraft:green_candle", 956}, {"minecraft:green_candle_cake", 973},
				{"minecraft:green_carpet", 549}, {"minecraft:green_concrete", 721}, {"minecraft:green_concrete_powder", 737}, {"minecraft:green_glazed_terracotta", 705},
				{"minecraft:green_shulker_box", 689}, {"minecraft:green_stained_glass", 312}, {"minecraft:green_stained_glass_pane", 511}, {"minecraft:green_terracotta", 495},
				{"minecraft:green_wall_banner", 590}, {"minecraft:green_wool", 153}, {"minecraft:grindstone", 842}, {"minecraft:hanging_roots", 1118},
				{"minecraft:hay_block", 535}, {"minecraft:heavy_core", 1157}, {"minecraft:heavy_weighted_pressure_plate", 470}, {"minecraft:honey_block", 911},
				{"minecraft:honeycomb_block", 912}, {"minecraft:hopper", 475}, {"minecraft:horn_coral", 765}, {"minecraft:horn_coral_block", 755},
				{"minecraft:horn_coral_fan", 775}, {"minecraft:horn_coral_wall_fan", 785}, {"minecraft:ice", 276}, {"minecraft:infested_chiseled_stone_bricks", 336},
				{"minecraft:infested_cobblestone", 332}, {"minecraft:infested_cracked_stone_bricks", 335}, {"minecraft:infested_deepslate", 1141}, {"minecraft:infested_mossy_stone_bricks", 334},
				{"minecraft:infested_stone", 331}, {"minecraft:infested_stone_bricks", 333}, {"minecraft:iron_bars", 340}, {"minecraft:iron_block", 174},
				{"minecraft:iron_chain", 349}, {"minecraft:iron_door", 259}, {"minecraft:iron_ore", 44}, {"minecraft:iron_trapdoor", 524},
				{"minecraft:jack_o_lantern", 296}, {"minecraft:jigsaw", 904}, {"minecraft:jukebox", 282}, {"minecraft:jungle_button", 444},
				{"minecraft:jungle_door", 646}, {"minecraft:jungle_fence", 637}, {"minecraft:jungle_fence_gate", 628}, {"minecraft:jungle_hanging_sign", 238},
				{"minecraft:jungle_leaves", 91}, {"minecraft:jungle_log", 52}, {"minecraft:jungle_planks", 16}, {"minecraft:jungle_pressure_plate", 263},
				{"minecraft:jungle_sapling", 28}, {"minecraft:jungle_shelf", 185}, {"minecraft:jungle_sign", 214}, {"minecraft:jungle_slab", 600},
				{"minecraft:jungle_stairs", 405}, {"minecraft:jungle_trapdoor", 318}, {"minecraft:jungle_wall_hanging_sign", 250}, {"minecraft:jungle_wall_sign", 228},
				{"minecraft:jungle_wood", 74}, {"minecraft:kelp", 740}, {"minecraft:kelp_plant", 741}, {"minecraft:ladder", 220},
				{"minecraft:lantern", 847}, {"minecraft:lapis_block", 104}, {"minecraft:lapis_ore", 102}, {"minecraft:large_amethyst_bud", 979},
				{"minecraft:large_fern", 560}, {"minecraft:lava", 36}, {"minecraft:lava_cauldron", 388}, {"minecraft:leaf_litter", 1113},
				{"minecraft:lectern", 843}, {"minecraft:lever", 257}, {"minecraft:light", 523}, {"minecraft:light_blue_banner", 564},
				{"minecraft:light_blue_bed", 113}, {"minecraft:light_blue_candle", 946}, {"minecraft:light_blue_candle_cake", 963}, {"minecraft:light_blue_carpet", 539},
				{"minecraft:light_blue_concrete", 711}, {"minecraft:light_blue_concrete_powder", 727}, {"minecraft:light_blue_glazed_terracotta", 695}, {"minecraft:light_blue_shulker_box", 679},
				{"minecraft:light_blue_stained_glass", 302}, {"minecraft:light_blue_stained_glass_pane", 501}, {"minecraft:light_blue_terracotta", 485}, {"minecraft:light_blue_wall_banner", 580},
				{"minecraft:light_blue_wool", 143}, {"minecraft:light_gray_banner", 569}, {"minecraft:light_gray_bed", 118}, {"minecraft:light_gray_candle", 951},
				{"minecraft:light_gray_candle_cake", 968}, {"minecraft:light_gray_carpet", 544}, {"minecraft:light_gray_concrete", 716}, {"minecraft:light_gray_concrete_powder", 732},
				{"minecraft:light_gray_glazed_terracotta", 700}, {"minecraft:light_gray_shulker_box", 684}, {"minecraft:light_gray_stained_glass", 307}, {"minecraft:light_gray_stained_glass_pane", 506},
				{"minecraft:light_gray_terracotta", 490}, {"minecraft:light_gray_wall_banner", 585}, {"minecraft:light_gray_wool", 148}, {"minecraft:light_weighted_pressure_plate", 469},
				{"minecraft:lightning_rod", 1095}, {"minecraft:lilac", 556}, {"minecraft:lily_of_the_valley", 170}, {"minecraft:lily_pad", 373},
				{"minecraft:lime_banner", 566}, {"minecraft:lime_bed", 115}, {"minecraft:lime_candle", 948}, {"minecraft:lime_candle_cake", 965},
				{"minecraft:lime_carpet", 541}, {"minecraft:lime_concrete", 713}, {"minecraft:lime_concrete_powder", 729}, {"minecraft:lime_glazed_terracotta", 697},
				{"minecraft:lime_shulker_box", 681}, {"minecraft:lime_stained_glass", 304}, {"minecraft:lime_stained_glass_pane", 503}, {"minecraft:lime_terracotta", 487},
				{"minecraft:lime_wall_banner", 582}, {"minecraft:lime_wool", 145}, {"minecraft:lodestone", 921}, {"minecraft:loom", 836},
				{"minecraft:magenta_banner", 563}, {"minecraft:magenta_bed", 112}, {"minecraft:magenta_candle", 945}, {"minecraft:magenta_candle_cake", 962},
				{"minecraft:magenta_carpet", 538}, {"minecraft:magenta_concrete", 710}, {"minecraft:magenta_concrete_powder", 726}, {"minecraft:magenta_glazed_terracotta", 694},
				{"minecraft:magenta_shulker_box", 678}, {"minecraft:magenta_stained_glass", 301}, {"minecraft:magenta_stained_glass_pane", 500}, {"minecraft:magenta_terracotta", 484},
				{"minecraft:magenta_wall_banner", 579}, {"minecraft:magenta_wool", 142}, {"minecraft:magma_block", 669}, {"minecraft:mangrove_button", 449},
				{"minecraft:mangrove_door", 651}, {"minecraft:mangrove_fence", 642}, {"minecraft:mangrove_fence_gate", 633}, {"minecraft:mangrove_hanging_sign", 243},
				{"minecraft:mangrove_leaves", 96}, {"minecraft:mangrove_log", 57}, {"minecraft:mangrove_planks", 22}, {"minecraft:mangrove_pressure_plate", 268},
				{"minecraft:mangrove_propagule", 33}, {"minecraft:mangrove_roots", 58}, {"minecraft:mangrove_shelf", 186}, {"minecraft:mangrove_sign", 217},
				{"minecraft:mangrove_slab", 605}, {"minecraft:mangrove_stairs", 518}, {"minecraft:mangrove_trapdoor", 323}, {"minecraft:mangrove_wall_hanging_sign", 253},
				{"minecraft:mangrove_wall_sign", 231}, {"minecraft:mangrove_wood", 78}, {"minecraft:medium_amethyst_bud", 980}, {"minecraft:melon", 360},
				{"minecraft:melon_stem", 364}, {"minecraft:moss_block", 1114}, {"minecraft:moss_carpet", 1110}, {"minecraft:mossy_cobblestone", 191},
				{"minecraft:mossy_cobblestone_slab", 813}, {"minecraft:mossy_cobblestone_stairs", 799}, {"minecraft:mossy_cobblestone_wall", 409}, {"minecraft:mossy_stone_brick_slab", 811},
				{"minecraft:mossy_stone_brick_stairs", 797}, {"minecraft:mossy_stone_brick_wall", 825}, {"minecraft:mossy_stone_bricks", 326}, {"minecraft:moving_piston", 156},
				{"minecraft:mud", 1120}, {"minecraft:mud_brick_slab", 616}, {"minecraft:mud_brick_stairs", 371}, {"minecraft:mud_brick_wall", 828},
				{"minecraft:mud_bricks", 330}, {"minecraft:muddy_mangrove_roots", 59}, {"minecraft:mushroom_stem", 339}, {"minecraft:mycelium", 372},
				{"minecraft:nether_brick_fence", 381}, {"minecraft:nether_brick_slab", 617}, {"minecraft:nether_brick_stairs", 382}, {"minecraft:nether_brick_wall", 829},
				{"minecraft:nether_bricks", 380}, {"minecraft:nether_gold_ore", 48}, {"minecraft:nether_portal", 294}, {"minecraft:nether_quartz_ore", 474},
				{"minecraft:nether_sprouts", 868}, {"minecraft:nether_wart", 383}, {"minecraft:nether_wart_block", 670}, {"minecraft:netherite_block", 913},
				{"minecraft:netherrack", 284}, {"minecraft:note_block", 109}, {"minecraft:oak_button", 441}, {"minecraft:oak_door", 219},
				{"minecraft:oak_fence", 283}, {"minecraft:oak_fence_gate", 368}, {"minecraft:oak_hanging_sign", 233}, {"minecraft:oak_leaves", 88},
				{"minecraft:oak_log", 49}, {"minecraft:oak_planks", 13}, {"minecraft:oak_pressure_plate", 260}, {"minecraft:oak_sapling", 25},
				{"minecraft:oak_shelf", 187}, {"minecraft:oak_sign", 209}, {"minecraft:oak_slab", 597}, {"minecraft:oak_stairs", 199},
				{"minecraft:oak_trapdoor", 315}, {"minecraft:oak_wall_hanging_sign", 245}, {"minecraft:oak_wall_sign", 223}, {"minecraft:oak_wood", 71},
				{"minecraft:observer", 674}, {"minecraft:obsidian", 192}, {"minecraft:ochre_froglight", 1148}, {"minecraft:open_eyeblossom", 1161},
				{"minecraft:orange_banner", 562}, {"minecraft:orange_bed", 111}, {"minecraft:orange_candle", 944}, {"minecraft:orange_candle_cake", 961},
				{"minecraft:orange_carpet", 537}, {"minecraft:orange_concrete", 709}, {"minecraft:orange_concrete_powder", 725}, {"minecraft:orange_glazed_terracotta", 693},
				{"minecraft:orange_shulker_box", 677}, {"minecraft:orange_stained_glass", 300}, {"minecraft:orange_stained_glass_pane", 499}, {"minecraft:orange_terracotta", 483},
				{"minecraft:orange_tulip", 164}, {"minecraft:orange_wall_banner", 578}, {"minecraft:orange_wool", 141}, {"minecraft:oxeye_daisy", 167},
				{"minecraft:oxidized_chiseled_copper", 1015}, {"minecraft:oxidized_copper", 1008}, {"minecraft:oxidized_copper_bars", 344}, {"minecraft:oxidized_copper_bulb", 1074},
				{"minecraft:oxidized_copper_chain", 353}, {"minecraft:oxidized_copper_chest", 1082}, {"minecraft:oxidized_copper_door", 1049}, {"minecraft:oxidized_copper_golem_statue", 1090},
				{"minecraft:oxidized_copper_grate", 1066}, {"minecraft:oxidized_copper_lantern", 852}, {"minecraft:oxidized_copper_trapdoor", 1057}, {"minecraft:oxidized_cut_copper", 1011},
				{"minecraft:oxidized_cut_copper_slab", 1027}, {"minecraft:oxidized_cut_copper_stairs", 1023}, {"minecraft:oxidized_lightning_rod", 1098}, {"minecraft:packed_ice", 554},
				{"minecraft:packed_mud", 329}, {"minecraft:pale_hanging_moss", 1160}, {"minecraft:pale_moss_block", 1158}, {"minecraft:pale_moss_carpet", 1159},
				{"minecraft:pale_oak_button", 448}, {"minecraft:pale_oak_door", 650}, {"minecraft:pale_oak_fence", 641}, {"minecraft:pale_oak_fence_gate", 632},
				{"minecraft:pale_oak_hanging_sign", 240}, {"minecraft:pale_oak_leaves", 95}, {"minecraft:pale_oak_log", 56}, {"minecraft:pale_oak_planks", 21},
				{"minecraft:pale_oak_pressure_plate", 267}, {"minecraft:pale_oak_sapling", 32}, {"minecraft:pale_oak_shelf", 188}, {"minecraft:pale_oak_sign", 216},
				{"minecraft:pale_oak_slab", 604}, {"minecraft:pale_oak_stairs", 517}, {"minecraft:pale_oak_trapdoor", 322}, {"minecraft:pale_oak_wall_hanging_sign", 252},
				{"minecraft:pale_oak_wall_sign", 230}, {"minecraft:pale_oak_wood", 20}, {"minecraft:pearlescent_froglight", 1150}, {"minecraft:peony", 558},
				{"minecraft:petrified_oak_slab", 612}, {"minecraft:piglin_head", 463}, {"minecraft:piglin_wall_head", 464}, {"minecraft:pink_banner", 567},
				{"minecraft:pink_bed", 116}, {"minecraft:pink_candle", 949}, {"minecraft:pink_candle_cake", 966}, {"minecraft:pink_carpet", 542},
				{"minecraft:pink_concrete", 714}, {"minecraft:pink_concrete_powder", 730}, {"minecraft:pink_glazed_terracotta", 698}, {"minecraft:pink_petals", 1111},
				{"minecraft:pink_shulker_box", 682}, {"minecraft:pink_stained_glass", 305}, {"minecraft:pink_stained_glass_pane", 504}, {"minecraft:pink_terracotta", 488},
				{"minecraft:pink_tulip", 166}, {"minecraft:pink_wall_banner", 583}, {"minecraft:pink_wool", 146}, {"minecraft:piston", 138},
				{"minecraft:piston_head", 139}, {"minecraft:pitcher_crop", 661}, {"minecraft:pitcher_plant", 662}, {"minecraft:player_head", 457},
				{"minecraft:player_wall_head", 458}, {"minecraft:podzol", 11}, {"minecraft:pointed_dripstone", 1103}, {"minecraft:polished_andesite", 7},
				{"minecraft:polished_andesite_slab", 820}, {"minecraft:polished_andesite_stairs", 807}, {"minecraft:polished_basalt", 288}, {"minecraft:polished_blackstone", 926},
				{"minecraft:polished_blackstone_brick_slab", 930}, {"minecraft:polished_blackstone_brick_stairs", 931}, {"minecraft:polished_blackstone_brick_wall", 932}, {"minecraft:polished_blackstone_bricks", 927},
				{"minecraft:polished_blackstone_button", 937}, {"minecraft:polished_blackstone_pressure_plate", 936}, {"minecraft:polished_blackstone_slab", 935}, {"minecraft:polished_blackstone_stairs", 934},
				{"minecraft:polished_blackstone_wall", 938}, {"minecraft:polished_deepslate", 1126}, {"minecraft:polished_deepslate_slab", 1128}, {"minecraft:polished_deepslate_stairs", 1127},
				{"minecraft:polished_deepslate_wall", 1129}, {"minecraft:polished_diorite", 5}, {"minecraft:polished_diorite_slab", 812}, {"minecraft:polished_diorite_stairs", 798},
				{"minecraft:polished_granite", 3}, {"minecraft:polished_granite_slab", 809}, {"minecraft:polished_granite_stairs", 795}, {"minecraft:polished_tuff", 986},
				{"minecraft:polished_tuff_slab", 987}, {"minecraft:polished_tuff_stairs", 988}, {"minecraft:polished_tuff_wall", 989}, {"minecraft:poppy", 159},
				{"minecraft:potatoes", 440}, {"minecraft:potted_acacia_sapling", 416}, {"minecraft:potted_allium", 425}, {"minecraft:potted_azalea_bush", 1146},
				{"minecraft:potted_azure_bluet", 426}, {"minecraft:potted_bamboo", 791}, {"minecraft:potted_birch_sapling", 414}, {"minecraft:potted_blue_orchid", 424},
				{"minecraft:potted_brown_mushroom", 436}, {"minecraft:potted_cactus", 438}, {"minecraft:potted_cherry_sapling", 417}, {"minecraft:potted_closed_eyeblossom", 1164},
				{"minecraft:potted_cornflower", 432}, {"minecraft:potted_crimson_fungus", 917}, {"minecraft:potted_crimson_roots", 919}, {"minecraft:potted_dandelion", 422},
				{"minecraft:potted_dark_oak_sapling", 418}, {"minecraft:potted_dead_bush", 437}, {"minecraft:potted_fern", 421}, {"minecraft:potted_flowering_azalea_bush", 1147},
				{"minecraft:potted_jungle_sapling", 415}, {"minecraft:potted_lily_of_the_valley", 433}, {"minecraft:potted_mangrove_propagule", 420}, {"minecraft:potted_oak_sapling", 412},
				{"minecraft:potted_open_eyeblossom", 1163}, {"minecraft:potted_orange_tulip", 428}, {"minecraft:potted_oxeye_daisy", 431}, {"minecraft:potted_pale_oak_sapling", 419},
				{"minecraft:potted_pink_tulip", 430}, {"minecraft:potted_poppy", 423}, {"minecraft:potted_red_mushroom", 435}, {"minecraft:potted_red_tulip", 427},
				{"minecraft:potted_spruce_sapling", 413}, {"minecraft:potted_torchflower", 411}, {"minecraft:potted_warped_fungus", 918}, {"minecraft:potted_warped_roots", 920},
				{"minecraft:potted_white_tulip", 429}, {"minecraft:potted_wither_rose", 434}, {"minecraft:powder_snow", 998}, {"minecraft:powder_snow_cauldron", 389},
				{"minecraft:powered_rail", 126}, {"minecraft:prismarine", 525}, {"minecraft:prismarine_brick_slab", 532}, {"minecraft:prismarine_brick_stairs", 529},
				{"minecraft:prismarine_bricks", 526}, {"minecraft:prismarine_slab", 531}, {"minecraft:prismarine_stairs", 528}, {"minecraft:prismarine_wall", 823},
				{"minecraft:pumpkin", 359}, {"minecraft:pumpkin_stem", 363}, {"minecraft:purple_banner", 571}, {"minecraft:purple_bed", 120},
				{"minecraft:purple_candle", 953}, {"minecraft:purple_candle_cake", 970}, {"minecraft:purple_carpet", 546}, {"minecraft:purple_concrete", 718},
				{"minecraft:purple_concrete_powder", 734}, {"minecraft:purple_glazed_terracotta", 702}, {"minecraft:purple_shulker_box", 686}, {"minecraft:purple_stained_glass", 309},
				{"minecraft:purple_stained_glass_pane", 508}, {"minecraft:purple_terracotta", 492}, {"minecraft:purple_wall_banner", 587}, {"minecraft:purple_wool", 150},
				{"minecraft:purpur_block", 656}, {"minecraft:purpur_pillar", 657}, {"minecraft:purpur_slab", 621}, {"minecraft:purpur_stairs", 658},
				{"minecraft:quartz_block", 476}, {"minecraft:quartz_bricks", 941}, {"minecraft:quartz_pillar", 478}, {"minecraft:quartz_slab", 618},
				{"minecraft:quartz_stairs", 479}, {"minecraft:rail", 221}, {"minecraft:raw_copper_block", 1144}, {"minecraft:raw_gold_block", 1145},
				{"minecraft:raw_iron_block", 1143}, {"minecraft:red_banner", 575}, {"minecraft:red_bed", 124}, {"minecraft:red_candle", 957},
				{"minecraft:red_candle_cake", 974}, {"minecraft:red_carpet", 550}, {"minecraft:red_concrete", 722}, {"minecraft:red_concrete_powder", 738},
				{"minecraft:red_glazed_terracotta", 706}, {"minecraft:red_mushroom", 172}, {"minecraft:red_mushroom_block", 338}, {"minecraft:red_nether_brick_slab", 819},
				{"minecraft:red_nether_brick_stairs", 806}, {"minecraft:red_nether_brick_wall", 831}, {"minecraft:red_nether_bricks", 671}, {"minecraft:red_sand", 39},
				{"minecraft:red_sandstone", 593}, {"minecraft:red_sandstone_slab", 619}, {"minecraft:red_sandstone_stairs", 596}, {"minecraft:red_sandstone_wall", 824},
				{"minecraft:red_shulker_box", 690}, {"minecraft:red_stained_glass", 313}, {"minecraft:red_stained_glass_pane", 512}, {"minecraft:red_terracotta", 496},
				{"minecraft:red_tulip", 163}, {"minecraft:red_wall_banner", 591}, {"minecraft:red_wool", 154}, {"minecraft:redstone_block", 473},
				{"minecraft:redstone_lamp", 394}, {"minecraft:redstone_ore", 270}, {"minecraft:redstone_torch", 272}, {"minecraft:redstone_wall_torch", 273},
				{"minecraft:redstone_wire", 201}, {"minecraft:reinforced_deepslate", 1152}, {"minecraft:repeater", 298}, {"minecraft:repeating_command_block", 666},
				{"minecraft:resin_block", 374}, {"minecraft:resin_brick_slab", 377}, {"minecraft:resin_brick_stairs", 376}, {"minecraft:resin_brick_wall", 378},
				{"minecraft:resin_bricks", 375}, {"minecraft:resin_clump", 367}, {"minecraft:respawn_anchor", 916}, {"minecraft:rooted_dirt", 1119},
				{"minecraft:rose_bush", 557}, {"minecraft:sand", 37}, {"minecraft:sandstone", 106}, {"minecraft:sandstone_slab", 610},
				{"minecraft:sandstone_stairs", 396}, {"minecraft:sandstone_wall", 832}, {"minecraft:scaffolding", 835}, {"minecraft:sculk", 1001},
				{"minecraft:sculk_catalyst", 1003}, {"minecraft:sculk_sensor", 999}, {"minecraft:sculk_shrieker", 1004}, {"minecraft:sculk_vein", 1002},
				{"minecraft:sea_lantern", 534}, {"minecraft:sea_pickle", 786}, {"minecraft:seagrass", 136}, {"minecraft:short_dry_grass", 134},
				{"minecraft:short_grass", 130}, {"minecraft:shroomlight", 875}, {"minecraft:shulker_box", 675}, {"minecraft:skeleton_skull", 451},
				{"minecraft:skeleton_wall_skull", 452}, {"minecraft:slime_block", 521}, {"minecraft:small_amethyst_bud", 981}, {"minecraft:small_dripleaf", 1117},
				{"minecraft:smithing_table", 844}, {"minecraft:smoker", 838}, {"minecraft:smooth_basalt", 1142}, {"minecraft:smooth_quartz", 624},
				{"minecraft:smooth_quartz_slab", 816}, {"minecraft:smooth_quartz_stairs", 803}, {"minecraft:smooth_red_sandstone", 625}, {"minecraft:smooth_red_sandstone_slab", 810},
				{"minecraft:smooth_red_sandstone_stairs", 796}, {"minecraft:smooth_sandstone", 623}, {"minecraft:smooth_sandstone_slab", 815}, {"minecraft:smooth_sandstone_stairs", 802},
				{"minecraft:smooth_stone", 622}, {"minecraft:smooth_stone_slab", 609}, {"minecraft:sniffer_egg", 744}, {"minecraft:snow", 275},
				{"minecraft:snow_block", 277}, {"minecraft:soul_campfire", 858}, {"minecraft:soul_fire", 196}, {"minecraft:soul_lantern", 848},
				{"minecraft:soul_sand", 285}, {"minecraft:soul_soil", 286}, {"minecraft:soul_torch", 289}, {"minecraft:soul_wall_torch", 290},
				{"minecraft:spawner", 197}, {"minecraft:sponge", 99}, {"minecraft:spore_blossom", 1107}, {"minecraft:spruce_button", 442},
				{"minecraft:spruce_door", 644}, {"minecraft:spruce_fence", 635}, {"minecraft:spruce_fence_gate", 626}, {"minecraft:spruce_hanging_sign", 234},
				{"minecraft:spruce_leaves", 89}, {"minecraft:spruce_log", 50}, {"minecraft:spruce_planks", 14}, {"minecraft:spruce_pressure_plate", 261},
				{"minecraft:spruce_sapling", 26}, {"minecraft:spruce_shelf", 189}, {"minecraft:spruce_sign", 210}, {"minecraft:spruce_slab", 598},
				{"minecraft:spruce_stairs", 403}, {"minecraft:spruce_trapdoor", 316}, {"minecraft:spruce_wall_hanging_sign", 246}, {"minecraft:spruce_wall_sign", 224},
				{"minecraft:spruce_wood", 72}, {"minecraft:sticky_piston", 128}, {"minecraft:stone", 1}, {"minecraft:stone_brick_slab", 615},
				{"minecraft:stone_brick_stairs", 370}, {"minecraft:stone_brick_wall", 827}, {"minecraft:stone_bricks", 325}, {"minecraft:stone_button", 274},
				{"minecraft:stone_pressure_plate", 258}, {"minecraft:stone_slab", 608}, {"minecraft:stone_stairs", 801}, {"minecraft:stonecutter", 845},
				{"minecraft:stripped_acacia_log", 64}, {"minecraft:stripped_acacia_wood", 83}, {"minecraft:stripped_bamboo_block", 70}, {"minecraft:stripped_birch_log", 62},
				{"minecraft:stripped_birch_wood", 81}, {"minecraft:stripped_cherry_log", 65}, {"minecraft:stripped_cherry_wood", 84}, {"minecraft:stripped_crimson_hyphae", 872},
				{"minecraft:stripped_crimson_stem", 870}, {"minecraft:stripped_dark_oak_log", 66}, {"minecraft:stripped_dark_oak_wood", 85}, {"minecraft:stripped_jungle_log", 63},
				{"minecraft:stripped_jungle_wood", 82}, {"minecraft:stripped_mangrove_log", 69}, {"minecraft:stripped_mangrove_wood", 87}, {"minecraft:stripped_oak_log", 68},
				{"minecraft:stripped_oak_wood", 79}, {"minecraft:stripped_pale_oak_log", 67}, {"minecraft:stripped_pale_oak_wood", 86}, {"minecraft:stripped_spruce_log", 61},
				{"minecraft:stripped_spruce_wood", 80}, {"minecraft:stripped_warped_hyphae", 863}, {"minecraft:stripped_warped_stem", 861}, {"minecraft:structure_block", 903},
				{"minecraft:structure_void", 673}, {"minecraft:sugar_cane", 281}, {"minecraft:sunflower", 555}, {"minecraft:suspicious_gravel", 41},
				{"minecraft:suspicious_sand", 38}, {"minecraft:sweet_berry_bush", 859}, {"minecraft:tall_dry_grass", 135}, {"minecraft:tall_grass", 559},
				{"minecraft:tall_seagrass", 137}, {"minecraft:target", 908}, {"minecraft:terracotta", 552}, {"minecraft:test_block", 905},
				{"minecraft:test_instance_block", 906}, {"minecraft:tinted_glass", 997}, {"minecraft:tnt", 176}, {"minecraft:torch", 193},
				{"minecraft:torchflower", 158}, {"minecraft:torchflower_crop", 660}, {"minecraft:trapped_chest", 468}, {"minecraft:trial_spawner", 1155},
				{"minecraft:tripwire", 401}, {"minecraft:tripwire_hook", 400}, {"minecraft:tube_coral", 761}, {"minecraft:tube_coral_block", 751},
				{"minecraft:tube_coral_fan", 771}, {"minecraft:tube_coral_wall_fan", 781}, {"minecraft:tuff", 982}, {"minecraft:tuff_brick_slab", 992},
				{"minecraft:tuff_brick_stairs", 993}, {"minecraft:tuff_brick_wall", 994}, {"minecraft:tuff_bricks", 991}, {"minecraft:tuff_slab", 983},
				{"minecraft:tuff_stairs", 984}, {"minecraft:tuff_wall", 985}, {"minecraft:turtle_egg", 743}, {"minecraft:twisting_vines", 878},
				{"minecraft:twisting_vines_plant", 879}, {"minecraft:vault", 1156}, {"minecraft:verdant_froglight", 1149}, {"minecraft:vine", 365},
				{"minecraft:void_air", 792}, {"minecraft:wall_torch", 194}, {"minecraft:warped_button", 896}, {"minecraft:warped_door", 898},
				{"minecraft:warped_fence", 888}, {"minecraft:warped_fence_gate", 892}, {"minecraft:warped_fungus", 865}, {"minecraft:warped_hanging_sign", 242},
				{"minecraft:warped_hyphae", 862}, {"minecraft:warped_nylium", 864}, {"minecraft:warped_planks", 882}, {"minecraft:warped_pressure_plate", 886},
				{"minecraft:warped_roots", 867}, {"minecraft:warped_shelf", 190}, {"minecraft:warped_sign", 900}, {"minecraft:warped_slab", 884},
				{"minecraft:warped_stairs", 894}, {"minecraft:warped_stem", 860}, {"minecraft:warped_trapdoor", 890}, {"minecraft:warped_wall_hanging_sign", 255},
				{"minecraft:warped_wall_sign", 902}, {"minecraft:warped_wart_block", 866}, {"minecraft:water", 35}, {"minecraft:water_cauldron", 387},
				{"minecraft:waxed_chiseled_copper", 1022}, {"minecraft:waxed_copper_bars", 345}, {"minecraft:waxed_copper_block", 1031}, {"minecraft:waxed_copper_bulb", 1075},
				{"minecraft:waxed_copper_chain", 354}, {"minecraft:waxed_copper_chest", 1083}, {"minecraft:waxed_copper_door", 1051}, {"minecraft:waxed_copper_golem_statue", 1091},
				{"minecraft:waxed_copper_grate", 1067}, {"minecraft:waxed_copper_lantern", 853}, {"minecraft:waxed_copper_trapdoor", 1059}, {"minecraft:waxed_cut_copper", 1038},
				{"minecraft:waxed_cut_copper_slab", 1046}, {"minecraft:waxed_cut_copper_stairs", 1042}, {"minecraft:waxed_exposed_chiseled_copper", 1021}, {"minecraft:waxed_exposed_copper", 1033},
				{"minecraft:waxed_exposed_copper_bars", 346}, {"minecraft:waxed_exposed_copper_bulb", 1076}, {"minecraft:waxed_exposed_copper_chain", 355}, {"minecraft:waxed_exposed_copper_chest", 1084},
				{"minecraft:waxed_exposed_copper_door", 1052}, {"minecraft:waxed_exposed_copper_golem_statue", 1092}, {"minecraft:waxed_exposed_copper_grate", 1068}, {"minecraft:waxed_exposed_copper_lantern", 854},
				{"minecraft:waxed_exposed_copper_trapdoor", 1060}, {"minecraft:waxed_exposed_cut_copper", 1037}, {"minecraft:waxed_exposed_cut_copper_slab", 1045}, {"minecraft:waxed_exposed_cut_copper_stairs", 1041},
				{"minecraft:waxed_exposed_lightning_rod", 1100}, {"minecraft:waxed_lightning_rod", 1099}, {"minecraft:waxed_oxidized_chiseled_copper", 1019}, {"minecraft:waxed_oxidized_copper", 1034},
				{"minecraft:waxed_oxidized_copper_bars", 348}, {"minecraft:waxed_oxidized_copper_bulb", 1078}, {"minecraft:waxed_oxidized_copper_chain", 357}, {"minecraft:waxed_oxidized_copper_chest", 1086},
				{"minecraft:waxed_oxidized_copper_door", 1053}, {"minecraft:waxed_oxidized_copper_golem_statue", 1094}, {"minecraft:waxed_oxidized_copper_grate", 1070}, {"minecraft:waxed_oxidized_copper_lantern", 856},
				{"minecraft:waxed_oxidized_copper_trapdoor", 1061}, {"minecraft:waxed_oxidized_cut_copper", 1035}, {"minecraft:waxed_oxidized_cut_copper_slab", 1043}, {"minecraft:waxed_oxidized_cut_copper_stairs", 1039},
				{"minecraft:waxed_oxidized_lightning_rod", 1102}, {"minecraft:waxed_weathered_chiseled_copper", 1020}, {"minecraft:waxed_weathered_copper", 1032}, {"minecraft:waxed_weathered_copper_bars", 347},
				{"minecraft:waxed_weathered_copper_bulb", 1077}, {"minecraft:waxed_weathered_copper_chain", 356}, {"minecraft:waxed_weathered_copper_chest", 1085}, {"minecraft:waxed_weathered_copper_door", 1054},
				{"minecraft:waxed_weathered_copper_golem_statue", 1093}, {"minecraft:waxed_weathered_copper_grate", 1069}, {"minecraft:waxed_weathered_copper_lantern", 855}, {"minecraft:waxed_weathered_copper_trapdoor", 1062},
				{"minecraft:waxed_weathered_cut_copper", 1036}, {"minecraft:waxed_weathered_cut_copper_slab", 1044}, {"minecraft:waxed_weathered_cut_copper_stairs", 1040}, {"minecraft:waxed_weathered_lightning_rod", 1101},
				{"minecraft:weathered_chiseled_copper", 1016}, {"minecraft:weathered_copper", 1007}, {"minecraft:weathered_copper_bars", 343}, {"minecraft:weathered_copper_bulb", 1073},
				{"minecraft:weathered_copper_chain", 352}, {"minecraft:weathered_copper_chest", 1081}, {"minecraft:weathered_copper_door", 1050}, {"minecraft:weathered_copper_golem_statue", 1089},
				{"minecraft:weathered_copper_grate", 1065}, {"minecraft:weathered_copper_lantern", 851}, {"minecraft:weathered_copper_trapdoor", 1058}, {"minecraft:weathered_cut_copper", 1012},
				{"minecraft:weathered_cut_copper_slab", 1028}, {"minecraft:weathered_cut_copper_stairs", 1024}, {"minecraft:weathered_lightning_rod", 1097}, {"minecraft:weeping_vines", 876},
				{"minecraft:weeping_vines_plant", 877}, {"minecraft:wet_sponge", 100}, {"minecraft:wheat", 206}, {"minecraft:white_banner", 561},
				{"minecraft:white_bed", 110}, {"minecraft:white_candle", 943}, {"minecraft:white_candle_cake", 960}, {"minecraft:white_carpet", 536},
				{"minecraft:white_concrete", 708}, {"minecraft:white_concrete_powder", 724}, {"minecraft:white_glazed_terracotta", 692}, {"minecraft:white_shulker_box", 676},
				{"minecraft:white_stained_glass", 299}, {"minecraft:white_stained_glass_pane", 498}, {"minecraft:white_terracotta", 482}, {"minecraft:white_tulip", 165},
				{"minecraft:white_wall_banner", 577}, {"minecraft:white_wool", 140}, {"minecraft:wildflowers", 1112}, {"minecraft:wither_rose", 169},
				{"minecraft:wither_skeleton_skull", 453}, {"minecraft:wither_skeleton_wall_skull", 454}, {"minecraft:yellow_banner", 565}, {"minecraft:yellow_bed", 114},
				{"minecraft:yellow_candle", 947}, {"minecraft:yellow_candle_cake", 964}, {"minecraft:yellow_carpet", 540}, {"minecraft:yellow_concrete", 712},
				{"minecraft:yellow_concrete_powder", 728}, {"minecraft:yellow_glazed_terracotta", 696}, {"minecraft:yellow_shulker_box", 680}, {"minecraft:yellow_stained_glass", 303},
				{"minecraft:yellow_stained_glass_pane", 502}, {"minecraft:yellow_terracotta", 486}, {"minecraft:yellow_wall_banner", 581}, {"minecraft:yellow_wool", 144},
				{"minecraft:zombie_head", 455}, {"minecraft:zombie_wall_head", 456}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getBlockEntityType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:banner", 20}, {"minecraft:barrel", 27}, {"minecraft:beacon", 15}, {"minecraft:bed", 25},
				{"minecraft:beehive", 34}, {"minecraft:bell", 31}, {"minecraft:blast_furnace", 29}, {"minecraft:brewing_stand", 12},
				{"minecraft:brushable_block", 41}, {"minecraft:calibrated_sculk_sensor", 36}, {"minecraft:campfire", 33}, {"minecraft:chest", 1},
				{"minecraft:chiseled_bookshelf", 39}, {"minecraft:command_block", 23}, {"minecraft:comparator", 19}, {"minecraft:conduit", 26},
				{"minecraft:copper_golem_statue", 48}, {"minecraft:crafter", 43}, {"minecraft:creaking_heart", 10}, {"minecraft:daylight_detector", 17},
				{"minecraft:decorated_pot", 42}, {"minecraft:dispenser", 5}, {"minecraft:dropper", 6}, {"minecraft:enchanting_table", 13},
				{"minecraft:end_gateway", 22}, {"minecraft:end_portal", 14}, {"minecraft:ender_chest", 3}, {"minecraft:furnace", 0},
				{"minecraft:hanging_sign", 8}, {"minecraft:hopper", 18}, {"minecraft:jigsaw", 32}, {"minecraft:jukebox", 4},
				{"minecraft:lectern", 30}, {"minecraft:mob_spawner", 9}, {"minecraft:piston", 11}, {"minecraft:sculk_catalyst", 37},
				{"minecraft:sculk_sensor", 35}, {"minecraft:sculk_shrieker", 38}, {"minecraft:shelf", 40}, {"minecraft:shulker_box", 24},
				{"minecraft:sign", 7}, {"minecraft:skull", 16}, {"minecraft:smoker", 28}, {"minecraft:structure_block", 21},
				{"minecraft:test_block", 46}, {"minecraft:test_instance_block", 47}, {"minecraft:trapped_chest", 2}, {"minecraft:trial_spawner", 44},
				{"minecraft:vault", 45}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getBlockPredicateType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:all_of", 9}, {"minecraft:any_of", 8}, {"minecraft:has_sturdy_face", 3}, {"minecraft:inside_world_bounds", 7},
				{"minecraft:matching_block_tag", 1}, {"minecraft:matching_blocks", 0}, {"minecraft:matching_fluids", 2}, {"minecraft:not", 10},
				{"minecraft:replaceable", 5}, {"minecraft:solid", 4}, {"minecraft:true", 11}, {"minecraft:unobstructed", 12},
				{"minecraft:would_survive", 6}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getBlockType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:air", 1}, {"minecraft:amethyst", 2}, {"minecraft:amethyst_cluster", 3}, {"minecraft:anvil", 4},
				{"minecraft:attached_stem", 5}, {"minecraft:azalea", 6}, {"minecraft:bamboo_sapling", 7}, {"minecraft:bamboo_stalk", 8},
				{"minecraft:banner", 9}, {"minecraft:barrel", 10}, {"minecraft:barrier", 11}, {"minecraft:base_coral_fan", 12},
				{"minecraft:base_coral_plant", 13}, {"minecraft:base_coral_wall_fan", 14}, {"minecraft:beacon", 15}, {"minecraft:bed", 16},
				{"minecraft:beehive", 17}, {"minecraft:beetroot", 18}, {"minecraft:bell", 19}, {"minecraft:big_dripleaf", 20},
				{"minecraft:big_dripleaf_stem", 21}, {"minecraft:blast_furnace", 22}, {"minecraft:block", 0}, {"minecraft:bonemealable_feature_placer", 86},
				{"minecraft:brewing_stand", 23}, {"minecraft:brushable", 24}, {"minecraft:bubble_column", 25}, {"minecraft:budding_amethyst", 26},
				{"minecraft:bush", 27}, {"minecraft:button", 28}, {"minecraft:cactus", 29}, {"minecraft:cactus_flower", 30},
				{"minecraft:cake", 31}, {"minecraft:calibrated_sculk_sensor", 32}, {"minecraft:campfire", 33}, {"minecraft:candle", 35},
				{"minecraft:candle_cake", 34}, {"minecraft:carpet", 36}, {"minecraft:carrot", 37}, {"minecraft:cartography_table", 38},
				{"minecraft:cauldron", 39}, {"minecraft:cave_vines", 40}, {"minecraft:cave_vines_plant", 41}, {"minecraft:ceiling_hanging_sign", 42},
				{"minecraft:chain", 43}, {"minecraft:chest", 44}, {"minecraft:chiseled_book_shelf", 45}, {"minecraft:chorus_flower", 46},
				{"minecraft:chorus_plant", 47}, {"minecraft:cocoa", 48}, {"minecraft:colored_falling", 49}, {"minecraft:command", 50},
				{"minecraft:comparator", 51}, {"minecraft:composter", 52}, {"minecraft:concrete_powder", 53}, {"minecraft:conduit", 54},
				{"minecraft:copper_bulb_block", 55}, {"minecraft:copper_chest", 56}, {"minecraft:copper_golem_statue", 57}, {"minecraft:coral", 58},
				{"minecraft:coral_fan", 59}, {"minecraft:coral_plant", 60}, {"minecraft:coral_wall_fan", 61}, {"minecraft:crafter", 62},
				{"minecraft:crafting_table", 63}, {"minecraft:creaking_heart", 196}, {"minecraft:crop", 64}, {"minecraft:crying_obsidian", 65},
				{"minecraft:daylight_detector", 66}, {"minecraft:decorated_pot", 68}, {"minecraft:detector_rail", 69}, {"minecraft:dirt_path", 70},
				{"minecraft:dispenser", 71}, {"minecraft:door", 72}, {"minecraft:double_plant", 73}, {"minecraft:dragon_egg", 74},
				{"minecraft:dried_ghast", 75}, {"minecraft:drop_experience", 76}, {"minecraft:dropper", 77}, {"minecraft:dry_vegetation", 67},
				{"minecraft:enchantment_table", 78}, {"minecraft:end_gateway", 80}, {"minecraft:end_portal", 81}, {"minecraft:end_portal_frame", 82},
				{"minecraft:end_rod", 83}, {"minecraft:ender_chest", 79}, {"minecraft:eyeblossom", 84}, {"minecraft:farm", 85},
				{"minecraft:fence", 87}, {"minecraft:fence_gate", 88}, {"minecraft:fire", 89}, {"minecraft:firefly_bush", 90},
				{"minecraft:flower", 91}, {"minecraft:flower_bed", 147}, {"minecraft:flower_pot", 92}, {"minecraft:frogspawn", 93},
				{"minecraft:frosted_ice", 94}, {"minecraft:fungus", 95}, {"minecraft:furnace", 96}, {"minecraft:glazed_terracotta", 97},
				{"minecraft:glow_lichen", 98}, {"minecraft:grass", 99}, {"minecraft:grindstone", 100}, {"minecraft:half_transparent", 101},
				{"minecraft:hanging_moss", 102}, {"minecraft:hanging_roots", 103}, {"minecraft:hay", 104}, {"minecraft:heavy_core", 105},
				{"minecraft:honey", 106}, {"minecraft:hopper", 107}, {"minecraft:huge_mushroom", 108}, {"minecraft:ice", 109},
				{"minecraft:infested", 110}, {"minecraft:infested_rotated_pillar", 111}, {"minecraft:iron_bars", 112}, {"minecraft:jack_o_lantern", 113},
				{"minecraft:jigsaw", 114}, {"minecraft:jukebox", 115}, {"minecraft:kelp", 116}, {"minecraft:kelp_plant", 117},
				{"minecraft:ladder", 118}, {"minecraft:lantern", 119}, {"minecraft:lava_cauldron", 120}, {"minecraft:layered_cauldron", 121},
				{"minecraft:leaf_litter", 122}, {"minecraft:lectern", 123}, {"minecraft:lever", 124}, {"minecraft:light", 125},
				{"minecraft:lightning_rod", 126}, {"minecraft:liquid", 127}, {"minecraft:loom", 128}, {"minecraft:magma", 129},
				{"minecraft:mangrove_leaves", 130}, {"minecraft:mangrove_propagule", 131}, {"minecraft:mangrove_roots", 132}, {"minecraft:mossy_carpet", 133},
				{"minecraft:moving_piston", 134}, {"minecraft:mud", 135}, {"minecraft:multiface", 136}, {"minecraft:mushroom", 137},
				{"minecraft:mycelium", 138}, {"minecraft:nether_portal", 139}, {"minecraft:nether_sprouts", 141}, {"minecraft:nether_wart", 142},
				{"minecraft:netherrack", 140}, {"minecraft:note", 143}, {"minecraft:nylium", 144}, {"minecraft:observer", 145},
				{"minecraft:piglinwallskull", 146}, {"minecraft:piston_base", 148}, {"minecraft:piston_head", 149}, {"minecraft:pitcher_crop", 150},
				{"minecraft:player_head", 151}, {"minecraft:player_wall_head", 152}, {"minecraft:pointed_dripstone", 153}, {"minecraft:potato", 154},
				{"minecraft:powder_snow", 155}, {"minecraft:powered", 156}, {"minecraft:powered_rail", 157}, {"minecraft:pressure_plate", 158},
				{"minecraft:pumpkin", 159}, {"minecraft:rail", 160}, {"minecraft:redstone_lamp", 161}, {"minecraft:redstone_ore", 162},
				{"minecraft:redstone_torch", 163}, {"minecraft:redstone_wall_torch", 164}, {"minecraft:redstone_wire", 165}, {"minecraft:repeater", 166},
				{"minecraft:respawn_anchor", 167}, {"minecraft:rooted_dirt", 168}, {"minecraft:roots", 169}, {"minecraft:rotated_pillar", 170},
				{"minecraft:sand", 172}, {"minecraft:sapling", 171}, {"minecraft:scaffolding", 173}, {"minecraft:sculk", 175},
				{"minecraft:sculk_catalyst", 174}, {"minecraft:sculk_sensor", 176}, {"minecraft:sculk_shrieker", 177}, {"minecraft:sculk_vein", 178},
				{"minecraft:sea_pickle", 180}, {"minecraft:seagrass", 179}, {"minecraft:shelf", 181}, {"minecraft:short_dry_grass", 182},
				{"minecraft:shulker_box", 183}, {"minecraft:skull", 184}, {"minecraft:slab", 185}, {"minecraft:slime", 186},
				{"minecraft:small_dripleaf", 187}, {"minecraft:smithing_table", 188}, {"minecraft:smoker", 189}, {"minecraft:sniffer_egg", 190},
				{"minecraft:snow_layer", 191}, {"minecraft:snowy_dirt", 192}, {"minecraft:soul_fire", 193}, {"minecraft:soul_sand", 194},
				{"minecraft:spawner", 195}, {"minecraft:sponge", 197}, {"minecraft:spore_blossom", 198}, {"minecraft:stained_glass", 200},
				{"minecraft:stained_glass_pane", 199}, {"minecraft:stair", 201}, {"minecraft:standing_sign", 202}, {"minecraft:stem", 203},
				{"minecraft:stonecutter", 204}, {"minecraft:structure", 205}, {"minecraft:structure_void", 206}, {"minecraft:sugar_cane", 207},
				{"minecraft:sweet_berry_bush", 208}, {"minecraft:tall_dry_grass", 209}, {"minecraft:tall_flower", 210}, {"minecraft:tall_grass", 211},
				{"minecraft:tall_seagrass", 212}, {"minecraft:target", 213}, {"minecraft:test", 214}, {"minecraft:test_instance", 215},
				{"minecraft:tinted_glass", 216}, {"minecraft:tinted_particle_leaves", 217}, {"minecraft:tnt", 218}, {"minecraft:torch", 220},
				{"minecraft:torchflower_crop", 219}, {"minecraft:transparent", 221}, {"minecraft:trapdoor", 222}, {"minecraft:trapped_chest", 223},
				{"minecraft:trial_spawner", 224}, {"minecraft:trip_wire_hook", 225}, {"minecraft:tripwire", 226}, {"minecraft:turtle_egg", 227},
				{"minecraft:twisting_vines", 229}, {"minecraft:twisting_vines_plant", 228}, {"minecraft:untinted_particle_leaves", 230}, {"minecraft:vault", 231},
				{"minecraft:vine", 232}, {"minecraft:wall", 238}, {"minecraft:wall_banner", 233}, {"minecraft:wall_hanging_sign", 234},
				{"minecraft:wall_sign", 235}, {"minecraft:wall_skull", 236}, {"minecraft:wall_torch", 237}, {"minecraft:waterlily", 239},
				{"minecraft:waterlogged_transparent", 240}, {"minecraft:weathering_copper_bar", 241}, {"minecraft:weathering_copper_bulb", 242}, {"minecraft:weathering_copper_chain", 243},
				{"minecraft:weathering_copper_chest", 244}, {"minecraft:weathering_copper_door", 245}, {"minecraft:weathering_copper_full", 246}, {"minecraft:weathering_copper_golem_statue", 247},
				{"minecraft:weathering_copper_grate", 248}, {"minecraft:weathering_copper_slab", 249}, {"minecraft:weathering_copper_stair", 250}, {"minecraft:weathering_copper_trap_door", 251},
				{"minecraft:weathering_lantern", 252}, {"minecraft:weathering_lightning_rod", 253}, {"minecraft:web", 254}, {"minecraft:weeping_vines", 256},
				{"minecraft:weeping_vines_plant", 255}, {"minecraft:weighted_pressure_plate", 257}, {"minecraft:wet_sponge", 258}, {"minecraft:wither_rose", 259},
				{"minecraft:wither_skull", 260}, {"minecraft:wither_wall_skull", 261}, {"minecraft:wool_carpet", 262}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getChunkStatus() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:biomes", 3}, {"minecraft:carvers", 6}, {"minecraft:empty", 0}, {"minecraft:features", 7},
				{"minecraft:full", 11}, {"minecraft:initialize_light", 8}, {"minecraft:light", 9}, {"minecraft:noise", 4},
				{"minecraft:spawn", 10}, {"minecraft:structure_references", 2}, {"minecraft:structure_starts", 1}, {"minecraft:surface", 5}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getCommandArgumentType() {
		static std::map<std::string, uint32_t> registry = {
				{"brigadier:bool", 0}, {"brigadier:double", 2}, {"brigadier:float", 1}, {"brigadier:integer", 3},
				{"brigadier:long", 4}, {"brigadier:string", 5}, {"minecraft:angle", 28}, {"minecraft:block_pos", 8},
				{"minecraft:block_predicate", 13}, {"minecraft:block_state", 12}, {"minecraft:color", 16}, {"minecraft:column_pos", 9},
				{"minecraft:component", 18}, {"minecraft:dialog", 55}, {"minecraft:dimension", 41}, {"minecraft:entity", 6},
				{"minecraft:entity_anchor", 38}, {"minecraft:float_range", 40}, {"minecraft:function", 37}, {"minecraft:game_profile", 7},
				{"minecraft:gamemode", 42}, {"minecraft:heightmap", 51}, {"minecraft:hex_color", 17}, {"minecraft:int_range", 39},
				{"minecraft:item_predicate", 15}, {"minecraft:item_slot", 34}, {"minecraft:item_slots", 35}, {"minecraft:item_stack", 14},
				{"minecraft:loot_modifier", 54}, {"minecraft:loot_predicate", 53}, {"minecraft:loot_table", 52}, {"minecraft:message", 20},
				{"minecraft:nbt_compound_tag", 21}, {"minecraft:nbt_path", 23}, {"minecraft:nbt_tag", 22}, {"minecraft:objective", 24},
				{"minecraft:objective_criteria", 25}, {"minecraft:operation", 26}, {"minecraft:particle", 27}, {"minecraft:resource", 46},
				{"minecraft:resource_key", 47}, {"minecraft:resource_location", 36}, {"minecraft:resource_or_tag", 44}, {"minecraft:resource_or_tag_key", 45},
				{"minecraft:resource_selector", 48}, {"minecraft:rotation", 29}, {"minecraft:score_holder", 31}, {"minecraft:scoreboard_slot", 30},
				{"minecraft:style", 19}, {"minecraft:swizzle", 32}, {"minecraft:team", 33}, {"minecraft:template_mirror", 49},
				{"minecraft:template_rotation", 50}, {"minecraft:time", 43}, {"minecraft:uuid", 56}, {"minecraft:vec2", 11},
				{"minecraft:vec3", 10}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getConsumeEffectType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:apply_effects", 0}, {"minecraft:clear_all_effects", 2}, {"minecraft:play_sound", 4}, {"minecraft:remove_effects", 1},
				{"minecraft:teleport_randomly", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getCreativeModeTab() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:building_blocks", 0}, {"minecraft:colored_blocks", 1}, {"minecraft:combat", 8}, {"minecraft:food_and_drinks", 9},
				{"minecraft:functional_blocks", 3}, {"minecraft:hotbar", 5}, {"minecraft:ingredients", 10}, {"minecraft:inventory", 13},
				{"minecraft:natural_blocks", 2}, {"minecraft:op_blocks", 12}, {"minecraft:redstone_blocks", 4}, {"minecraft:search", 6},
				{"minecraft:spawn_eggs", 11}, {"minecraft:tools_and_utilities", 7}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getCustomStat() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:animals_bred", 33}, {"minecraft:aviate_one_cm", 19}, {"minecraft:bell_ring", 69}, {"minecraft:boat_one_cm", 15},
				{"minecraft:clean_armor", 41}, {"minecraft:clean_banner", 42}, {"minecraft:clean_shulker_box", 43}, {"minecraft:climb_one_cm", 11},
				{"minecraft:crouch_one_cm", 7}, {"minecraft:damage_absorbed", 29}, {"minecraft:damage_blocked_by_shield", 28}, {"minecraft:damage_dealt", 24},
				{"minecraft:damage_dealt_absorbed", 25}, {"minecraft:damage_dealt_resisted", 26}, {"minecraft:damage_resisted", 30}, {"minecraft:damage_taken", 27},
				{"minecraft:deaths", 31}, {"minecraft:drop", 23}, {"minecraft:eat_cake_slice", 38}, {"minecraft:enchant_item", 54},
				{"minecraft:fall_one_cm", 10}, {"minecraft:fill_cauldron", 39}, {"minecraft:fish_caught", 35}, {"minecraft:fly_one_cm", 12},
				{"minecraft:happy_ghast_one_cm", 17}, {"minecraft:horse_one_cm", 18}, {"minecraft:inspect_dispenser", 48}, {"minecraft:inspect_dropper", 46},
				{"minecraft:inspect_hopper", 47}, {"minecraft:interact_with_anvil", 72}, {"minecraft:interact_with_beacon", 45}, {"minecraft:interact_with_blast_furnace", 62},
				{"minecraft:interact_with_brewingstand", 44}, {"minecraft:interact_with_campfire", 65}, {"minecraft:interact_with_cartography_table", 66}, {"minecraft:interact_with_crafting_table", 57},
				{"minecraft:interact_with_furnace", 56}, {"minecraft:interact_with_grindstone", 73}, {"minecraft:interact_with_lectern", 64}, {"minecraft:interact_with_loom", 67},
				{"minecraft:interact_with_smithing_table", 75}, {"minecraft:interact_with_smoker", 63}, {"minecraft:interact_with_stonecutter", 68}, {"minecraft:jump", 22},
				{"minecraft:leave_game", 0}, {"minecraft:minecart_one_cm", 14}, {"minecraft:mob_kills", 32}, {"minecraft:open_barrel", 61},
				{"minecraft:open_chest", 58}, {"minecraft:open_enderchest", 53}, {"minecraft:open_shulker_box", 60}, {"minecraft:pig_one_cm", 16},
				{"minecraft:play_noteblock", 49}, {"minecraft:play_record", 55}, {"minecraft:play_time", 1}, {"minecraft:player_kills", 34},
				{"minecraft:pot_flower", 51}, {"minecraft:raid_trigger", 70}, {"minecraft:raid_win", 71}, {"minecraft:sleep_in_bed", 59},
				{"minecraft:sneak_time", 5}, {"minecraft:sprint_one_cm", 8}, {"minecraft:strider_one_cm", 21}, {"minecraft:swim_one_cm", 20},
				{"minecraft:talked_to_villager", 36}, {"minecraft:target_hit", 74}, {"minecraft:time_since_death", 3}, {"minecraft:time_since_rest", 4},
				{"minecraft:total_world_time", 2}, {"minecraft:traded_with_villager", 37}, {"minecraft:trigger_trapped_chest", 52}, {"minecraft:tune_noteblock", 50},
				{"minecraft:use_cauldron", 40}, {"minecraft:walk_on_water_one_cm", 9}, {"minecraft:walk_one_cm", 6}, {"minecraft:walk_under_water_one_cm", 13}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDataComponentPredicateType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:attribute_modifiers", 11}, {"minecraft:bundle_contents", 6}, {"minecraft:container", 5}, {"minecraft:custom_data", 4},
				{"minecraft:damage", 0}, {"minecraft:enchantments", 1}, {"minecraft:firework_explosion", 7}, {"minecraft:fireworks", 8},
				{"minecraft:jukebox_playable", 13}, {"minecraft:potion_contents", 3}, {"minecraft:stored_enchantments", 2}, {"minecraft:trim", 12},
				{"minecraft:writable_book_content", 9}, {"minecraft:written_book_content", 10}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDataComponentType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:attribute_modifiers", 13}, {"minecraft:axolotl/variant", 91}, {"minecraft:banner_patterns", 63}, {"minecraft:base_color", 64},
				{"minecraft:bees", 68}, {"minecraft:block_entity_data", 51}, {"minecraft:block_state", 67}, {"minecraft:blocks_attacks", 33},
				{"minecraft:break_sound", 71}, {"minecraft:bucket_entity_data", 50}, {"minecraft:bundle_contents", 41}, {"minecraft:can_break", 12},
				{"minecraft:can_place_on", 11}, {"minecraft:cat/collar", 93}, {"minecraft:cat/variant", 92}, {"minecraft:charged_projectiles", 40},
				{"minecraft:chicken/variant", 86}, {"minecraft:consumable", 21}, {"minecraft:container", 66}, {"minecraft:container_loot", 70},
				{"minecraft:cow/variant", 85}, {"minecraft:creative_slot_lock", 17}, {"minecraft:custom_data", 0}, {"minecraft:custom_model_data", 14},
				{"minecraft:custom_name", 5}, {"minecraft:damage", 3}, {"minecraft:damage_resistant", 24}, {"minecraft:death_protection", 32},
				{"minecraft:debug_stick_state", 48}, {"minecraft:dyed_color", 35}, {"minecraft:enchantable", 27}, {"minecraft:enchantment_glint_override", 18},
				{"minecraft:enchantments", 10}, {"minecraft:entity_data", 49}, {"minecraft:equippable", 28}, {"minecraft:firework_explosion", 59},
				{"minecraft:fireworks", 60}, {"minecraft:food", 20}, {"minecraft:fox/variant", 76}, {"minecraft:frog/variant", 87},
				{"minecraft:glider", 30}, {"minecraft:horse/variant", 88}, {"minecraft:instrument", 52}, {"minecraft:intangible_projectile", 19},
				{"minecraft:item_model", 7}, {"minecraft:item_name", 6}, {"minecraft:jukebox_playable", 55}, {"minecraft:llama/variant", 90},
				{"minecraft:lock", 69}, {"minecraft:lodestone_tracker", 58}, {"minecraft:lore", 8}, {"minecraft:map_color", 36},
				{"minecraft:map_decorations", 38}, {"minecraft:map_id", 37}, {"minecraft:map_post_processing", 39}, {"minecraft:max_damage", 2},
				{"minecraft:max_stack_size", 1}, {"minecraft:mooshroom/variant", 82}, {"minecraft:note_block_sound", 62}, {"minecraft:ominous_bottle_amplifier", 54},
				{"minecraft:painting/variant", 89}, {"minecraft:parrot/variant", 78}, {"minecraft:pig/variant", 84}, {"minecraft:pot_decorations", 65},
				{"minecraft:potion_contents", 42}, {"minecraft:potion_duration_scale", 43}, {"minecraft:profile", 61}, {"minecraft:provides_banner_patterns", 56},
				{"minecraft:provides_trim_material", 53}, {"minecraft:rabbit/variant", 83}, {"minecraft:rarity", 9}, {"minecraft:recipes", 57},
				{"minecraft:repair_cost", 16}, {"minecraft:repairable", 29}, {"minecraft:salmon/size", 77}, {"minecraft:sheep/color", 94},
				{"minecraft:shulker/color", 95}, {"minecraft:stored_enchantments", 34}, {"minecraft:suspicious_stew_effects", 44}, {"minecraft:tool", 25},
				{"minecraft:tooltip_display", 15}, {"minecraft:tooltip_style", 31}, {"minecraft:trim", 47}, {"minecraft:tropical_fish/base_color", 80},
				{"minecraft:tropical_fish/pattern", 79}, {"minecraft:tropical_fish/pattern_color", 81}, {"minecraft:unbreakable", 4}, {"minecraft:use_cooldown", 23},
				{"minecraft:use_remainder", 22}, {"minecraft:villager/variant", 72}, {"minecraft:weapon", 26}, {"minecraft:wolf/collar", 75},
				{"minecraft:wolf/sound_variant", 74}, {"minecraft:wolf/variant", 73}, {"minecraft:writable_book_content", 45}, {"minecraft:written_book_content", 46}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDebugSubscription() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:bee_hives", 7}, {"minecraft:bees", 1}, {"minecraft:brains", 2}, {"minecraft:breezes", 3},
				{"minecraft:dedicated_server_tick_time", 0}, {"minecraft:entity_block_intersections", 6}, {"minecraft:entity_paths", 5}, {"minecraft:game_event_listeners", 13},
				{"minecraft:game_events", 15}, {"minecraft:goal_selectors", 4}, {"minecraft:neighbor_updates", 14}, {"minecraft:pois", 8},
				{"minecraft:raids", 11}, {"minecraft:redstone_wire_orientations", 9}, {"minecraft:structures", 12}, {"minecraft:village_sections", 10}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDecoratedPotPattern() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:angler", 0}, {"minecraft:archer", 1}, {"minecraft:arms_up", 2}, {"minecraft:blade", 3},
				{"minecraft:blank", 23}, {"minecraft:brewer", 4}, {"minecraft:burn", 5}, {"minecraft:danger", 6},
				{"minecraft:explorer", 7}, {"minecraft:flow", 8}, {"minecraft:friend", 9}, {"minecraft:guster", 10},
				{"minecraft:heart", 11}, {"minecraft:heartbreak", 12}, {"minecraft:howl", 13}, {"minecraft:miner", 14},
				{"minecraft:mourner", 15}, {"minecraft:plenty", 16}, {"minecraft:prize", 17}, {"minecraft:scrape", 18},
				{"minecraft:sheaf", 19}, {"minecraft:shelter", 20}, {"minecraft:skull", 21}, {"minecraft:snort", 22}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDialogActionType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:change_page", 4}, {"minecraft:copy_to_clipboard", 5}, {"minecraft:custom", 6}, {"minecraft:dynamic/custom", 8},
				{"minecraft:dynamic/run_command", 7}, {"minecraft:open_url", 0}, {"minecraft:run_command", 1}, {"minecraft:show_dialog", 3},
				{"minecraft:suggest_command", 2}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDialogBodyType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:item", 0}, {"minecraft:plain_message", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getDialogType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:confirmation", 4}, {"minecraft:dialog_list", 2}, {"minecraft:multi_action", 3}, {"minecraft:notice", 0},
				{"minecraft:server_links", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEnchantmentEffectComponentType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:ammo_use", 13}, {"minecraft:armor_effectiveness", 5}, {"minecraft:attributes", 9}, {"minecraft:block_experience", 21},
				{"minecraft:crossbow_charge_time", 24}, {"minecraft:crossbow_charging_sounds", 25}, {"minecraft:damage", 2}, {"minecraft:damage_immunity", 1},
				{"minecraft:damage_protection", 0}, {"minecraft:equipment_drops", 10}, {"minecraft:fishing_luck_bonus", 20}, {"minecraft:fishing_time_reduction", 19},
				{"minecraft:hit_block", 7}, {"minecraft:item_damage", 8}, {"minecraft:knockback", 4}, {"minecraft:location_changed", 11},
				{"minecraft:mob_experience", 22}, {"minecraft:post_attack", 6}, {"minecraft:prevent_armor_change", 28}, {"minecraft:prevent_equipment_drop", 27},
				{"minecraft:projectile_count", 17}, {"minecraft:projectile_piercing", 14}, {"minecraft:projectile_spawned", 15}, {"minecraft:projectile_spread", 16},
				{"minecraft:repair_with_xp", 23}, {"minecraft:smash_damage_per_fallen_block", 3}, {"minecraft:tick", 12}, {"minecraft:trident_return_acceleration", 18},
				{"minecraft:trident_sound", 26}, {"minecraft:trident_spin_attack_strength", 29}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEnchantmentEntityEffectType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:all_of", 0}, {"minecraft:apply_mob_effect", 1}, {"minecraft:change_item_damage", 2}, {"minecraft:damage_entity", 3},
				{"minecraft:explode", 4}, {"minecraft:ignite", 5}, {"minecraft:play_sound", 6}, {"minecraft:replace_block", 7},
				{"minecraft:replace_disk", 8}, {"minecraft:run_function", 9}, {"minecraft:set_block_properties", 10}, {"minecraft:spawn_particles", 11},
				{"minecraft:summon_entity", 12}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEnchantmentLevelBasedValueType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:clamped", 0}, {"minecraft:fraction", 1}, {"minecraft:levels_squared", 2}, {"minecraft:linear", 3},
				{"minecraft:lookup", 4}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEnchantmentLocationBasedEffectType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:all_of", 0}, {"minecraft:apply_mob_effect", 1}, {"minecraft:attribute", 2}, {"minecraft:change_item_damage", 3},
				{"minecraft:damage_entity", 4}, {"minecraft:explode", 5}, {"minecraft:ignite", 6}, {"minecraft:play_sound", 7},
				{"minecraft:replace_block", 8}, {"minecraft:replace_disk", 9}, {"minecraft:run_function", 10}, {"minecraft:set_block_properties", 11},
				{"minecraft:spawn_particles", 12}, {"minecraft:summon_entity", 13}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEnchantmentProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:by_cost", 0}, {"minecraft:by_cost_with_difficulty", 1}, {"minecraft:single", 2}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEnchantmentValueEffectType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:add", 0}, {"minecraft:all_of", 1}, {"minecraft:multiply", 2}, {"minecraft:remove_binomial", 3},
				{"minecraft:set", 4}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEntitySubPredicateType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:fishing_hook", 1}, {"minecraft:lightning", 0}, {"minecraft:player", 2}, {"minecraft:raider", 4},
				{"minecraft:sheep", 5}, {"minecraft:slime", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getEntityType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:acacia_boat", 0}, {"minecraft:acacia_chest_boat", 1}, {"minecraft:allay", 2}, {"minecraft:area_effect_cloud", 3},
				{"minecraft:armadillo", 4}, {"minecraft:armor_stand", 5}, {"minecraft:arrow", 6}, {"minecraft:axolotl", 7},
				{"minecraft:bamboo_chest_raft", 8}, {"minecraft:bamboo_raft", 9}, {"minecraft:bat", 10}, {"minecraft:bee", 11},
				{"minecraft:birch_boat", 12}, {"minecraft:birch_chest_boat", 13}, {"minecraft:blaze", 14}, {"minecraft:block_display", 15},
				{"minecraft:bogged", 16}, {"minecraft:breeze", 17}, {"minecraft:breeze_wind_charge", 18}, {"minecraft:camel", 19},
				{"minecraft:cat", 20}, {"minecraft:cave_spider", 21}, {"minecraft:cherry_boat", 22}, {"minecraft:cherry_chest_boat", 23},
				{"minecraft:chest_minecart", 24}, {"minecraft:chicken", 25}, {"minecraft:cod", 26}, {"minecraft:command_block_minecart", 28},
				{"minecraft:copper_golem", 27}, {"minecraft:cow", 29}, {"minecraft:creaking", 30}, {"minecraft:creeper", 31},
				{"minecraft:dark_oak_boat", 32}, {"minecraft:dark_oak_chest_boat", 33}, {"minecraft:dolphin", 34}, {"minecraft:donkey", 35},
				{"minecraft:dragon_fireball", 36}, {"minecraft:drowned", 37}, {"minecraft:egg", 38}, {"minecraft:elder_guardian", 39},
				{"minecraft:end_crystal", 44}, {"minecraft:ender_dragon", 42}, {"minecraft:ender_pearl", 43}, {"minecraft:enderman", 40},
				{"minecraft:endermite", 41}, {"minecraft:evoker", 45}, {"minecraft:evoker_fangs", 46}, {"minecraft:experience_bottle", 47},
				{"minecraft:experience_orb", 48}, {"minecraft:eye_of_ender", 49}, {"minecraft:falling_block", 50}, {"minecraft:fireball", 51},
				{"minecraft:firework_rocket", 52}, {"minecraft:fishing_bobber", 152}, {"minecraft:fox", 53}, {"minecraft:frog", 54},
				{"minecraft:furnace_minecart", 55}, {"minecraft:ghast", 56}, {"minecraft:giant", 58}, {"minecraft:glow_item_frame", 59},
				{"minecraft:glow_squid", 60}, {"minecraft:goat", 61}, {"minecraft:guardian", 62}, {"minecraft:happy_ghast", 57},
				{"minecraft:hoglin", 63}, {"minecraft:hopper_minecart", 64}, {"minecraft:horse", 65}, {"minecraft:husk", 66},
				{"minecraft:illusioner", 67}, {"minecraft:interaction", 68}, {"minecraft:iron_golem", 69}, {"minecraft:item", 70},
				{"minecraft:item_display", 71}, {"minecraft:item_frame", 72}, {"minecraft:jungle_boat", 73}, {"minecraft:jungle_chest_boat", 74},
				{"minecraft:leash_knot", 75}, {"minecraft:lightning_bolt", 76}, {"minecraft:lingering_potion", 103}, {"minecraft:llama", 77},
				{"minecraft:llama_spit", 78}, {"minecraft:magma_cube", 79}, {"minecraft:mangrove_boat", 80}, {"minecraft:mangrove_chest_boat", 81},
				{"minecraft:mannequin", 82}, {"minecraft:marker", 83}, {"minecraft:minecart", 84}, {"minecraft:mooshroom", 85},
				{"minecraft:mule", 86}, {"minecraft:oak_boat", 87}, {"minecraft:oak_chest_boat", 88}, {"minecraft:ocelot", 89},
				{"minecraft:ominous_item_spawner", 90}, {"minecraft:painting", 91}, {"minecraft:pale_oak_boat", 92}, {"minecraft:pale_oak_chest_boat", 93},
				{"minecraft:panda", 94}, {"minecraft:parrot", 95}, {"minecraft:phantom", 96}, {"minecraft:pig", 97},
				{"minecraft:piglin", 98}, {"minecraft:piglin_brute", 99}, {"minecraft:pillager", 100}, {"minecraft:player", 151},
				{"minecraft:polar_bear", 101}, {"minecraft:pufferfish", 104}, {"minecraft:rabbit", 105}, {"minecraft:ravager", 106},
				{"minecraft:salmon", 107}, {"minecraft:sheep", 108}, {"minecraft:shulker", 109}, {"minecraft:shulker_bullet", 110},
				{"minecraft:silverfish", 111}, {"minecraft:skeleton", 112}, {"minecraft:skeleton_horse", 113}, {"minecraft:slime", 114},
				{"minecraft:small_fireball", 115}, {"minecraft:sniffer", 116}, {"minecraft:snow_golem", 118}, {"minecraft:snowball", 117},
				{"minecraft:spawner_minecart", 119}, {"minecraft:spectral_arrow", 120}, {"minecraft:spider", 121}, {"minecraft:splash_potion", 102},
				{"minecraft:spruce_boat", 122}, {"minecraft:spruce_chest_boat", 123}, {"minecraft:squid", 124}, {"minecraft:stray", 125},
				{"minecraft:strider", 126}, {"minecraft:tadpole", 127}, {"minecraft:text_display", 128}, {"minecraft:tnt", 129},
				{"minecraft:tnt_minecart", 130}, {"minecraft:trader_llama", 131}, {"minecraft:trident", 132}, {"minecraft:tropical_fish", 133},
				{"minecraft:turtle", 134}, {"minecraft:vex", 135}, {"minecraft:villager", 136}, {"minecraft:vindicator", 137},
				{"minecraft:wandering_trader", 138}, {"minecraft:warden", 139}, {"minecraft:wind_charge", 140}, {"minecraft:witch", 141},
				{"minecraft:wither", 142}, {"minecraft:wither_skeleton", 143}, {"minecraft:wither_skull", 144}, {"minecraft:wolf", 145},
				{"minecraft:zoglin", 146}, {"minecraft:zombie", 147}, {"minecraft:zombie_horse", 148}, {"minecraft:zombie_villager", 149},
				{"minecraft:zombified_piglin", 150}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getFloatProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:clamped_normal", 2}, {"minecraft:constant", 0}, {"minecraft:trapezoid", 3}, {"minecraft:uniform", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getFluid() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:empty", 0}, {"minecraft:flowing_lava", 3}, {"minecraft:flowing_water", 1}, {"minecraft:lava", 4},
				{"minecraft:water", 2}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getGameEvent() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:block_activate", 0}, {"minecraft:block_attach", 1}, {"minecraft:block_change", 2}, {"minecraft:block_close", 3},
				{"minecraft:block_deactivate", 4}, {"minecraft:block_destroy", 5}, {"minecraft:block_detach", 6}, {"minecraft:block_open", 7},
				{"minecraft:block_place", 8}, {"minecraft:container_close", 9}, {"minecraft:container_open", 10}, {"minecraft:drink", 11},
				{"minecraft:eat", 12}, {"minecraft:elytra_glide", 13}, {"minecraft:entity_action", 20}, {"minecraft:entity_damage", 14},
				{"minecraft:entity_die", 15}, {"minecraft:entity_dismount", 16}, {"minecraft:entity_interact", 17}, {"minecraft:entity_mount", 18},
				{"minecraft:entity_place", 19}, {"minecraft:equip", 21}, {"minecraft:explode", 22}, {"minecraft:flap", 23},
				{"minecraft:fluid_pickup", 24}, {"minecraft:fluid_place", 25}, {"minecraft:hit_ground", 26}, {"minecraft:instrument_play", 27},
				{"minecraft:item_interact_finish", 28}, {"minecraft:item_interact_start", 29}, {"minecraft:jukebox_play", 30}, {"minecraft:jukebox_stop_play", 31},
				{"minecraft:lightning_strike", 32}, {"minecraft:note_block_play", 33}, {"minecraft:prime_fuse", 34}, {"minecraft:projectile_land", 35},
				{"minecraft:projectile_shoot", 36}, {"minecraft:resonate_1", 45}, {"minecraft:resonate_10", 54}, {"minecraft:resonate_11", 55},
				{"minecraft:resonate_12", 56}, {"minecraft:resonate_13", 57}, {"minecraft:resonate_14", 58}, {"minecraft:resonate_15", 59},
				{"minecraft:resonate_2", 46}, {"minecraft:resonate_3", 47}, {"minecraft:resonate_4", 48}, {"minecraft:resonate_5", 49},
				{"minecraft:resonate_6", 50}, {"minecraft:resonate_7", 51}, {"minecraft:resonate_8", 52}, {"minecraft:resonate_9", 53},
				{"minecraft:sculk_sensor_tendrils_clicking", 37}, {"minecraft:shear", 38}, {"minecraft:shriek", 39}, {"minecraft:splash", 40},
				{"minecraft:step", 41}, {"minecraft:swim", 42}, {"minecraft:teleport", 43}, {"minecraft:unequip", 44}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getHeightProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:biased_to_bottom", 2}, {"minecraft:constant", 0}, {"minecraft:trapezoid", 4}, {"minecraft:uniform", 1},
				{"minecraft:very_biased_to_bottom", 3}, {"minecraft:weighted_list", 5}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getIncomingRpcMethods() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:allowlist", 0}, {"minecraft:allowlist/add", 2}, {"minecraft:allowlist/clear", 4}, {"minecraft:allowlist/remove", 3},
				{"minecraft:allowlist/set", 1}, {"minecraft:bans", 5}, {"minecraft:bans/add", 7}, {"minecraft:bans/clear", 9},
				{"minecraft:bans/remove", 8}, {"minecraft:bans/set", 6}, {"minecraft:gamerules", 66}, {"minecraft:gamerules/update", 67},
				{"minecraft:ip_bans", 10}, {"minecraft:ip_bans/add", 12}, {"minecraft:ip_bans/clear", 14}, {"minecraft:ip_bans/remove", 13},
				{"minecraft:ip_bans/set", 11}, {"minecraft:operators", 17}, {"minecraft:operators/add", 19}, {"minecraft:operators/clear", 21},
				{"minecraft:operators/remove", 20}, {"minecraft:operators/set", 18}, {"minecraft:players", 15}, {"minecraft:players/kick", 16},
				{"minecraft:rpc.discover", 68}, {"minecraft:server/save", 23}, {"minecraft:server/status", 22}, {"minecraft:server/stop", 24},
				{"minecraft:server/system_message", 25}, {"minecraft:serversettings/accept_transfers", 54}, {"minecraft:serversettings/accept_transfers/set", 55}, {"minecraft:serversettings/allow_flight", 40},
				{"minecraft:serversettings/allow_flight/set", 41}, {"minecraft:serversettings/autosave", 26}, {"minecraft:serversettings/autosave/set", 27}, {"minecraft:serversettings/difficulty", 28},
				{"minecraft:serversettings/difficulty/set", 29}, {"minecraft:serversettings/enforce_allowlist", 30}, {"minecraft:serversettings/enforce_allowlist/set", 31}, {"minecraft:serversettings/entity_broadcast_range", 64},
				{"minecraft:serversettings/entity_broadcast_range/set", 65}, {"minecraft:serversettings/force_game_mode", 46}, {"minecraft:serversettings/force_game_mode/set", 47}, {"minecraft:serversettings/game_mode", 48},
				{"minecraft:serversettings/game_mode/set", 49}, {"minecraft:serversettings/hide_online_players", 60}, {"minecraft:serversettings/hide_online_players/set", 61}, {"minecraft:serversettings/max_players", 34},
				{"minecraft:serversettings/max_players/set", 35}, {"minecraft:serversettings/motd", 42}, {"minecraft:serversettings/motd/set", 43}, {"minecraft:serversettings/operator_user_permission_level", 58},
				{"minecraft:serversettings/operator_user_permission_level/set", 59}, {"minecraft:serversettings/pause_when_empty_seconds", 36}, {"minecraft:serversettings/pause_when_empty_seconds/set", 37}, {"minecraft:serversettings/player_idle_timeout", 38},
				{"minecraft:serversettings/player_idle_timeout/set", 39}, {"minecraft:serversettings/simulation_distance", 52}, {"minecraft:serversettings/simulation_distance/set", 53}, {"minecraft:serversettings/spawn_protection_radius", 44},
				{"minecraft:serversettings/spawn_protection_radius/set", 45}, {"minecraft:serversettings/status_heartbeat_interval", 56}, {"minecraft:serversettings/status_heartbeat_interval/set", 57}, {"minecraft:serversettings/status_replies", 62},
				{"minecraft:serversettings/status_replies/set", 63}, {"minecraft:serversettings/use_allowlist", 32}, {"minecraft:serversettings/use_allowlist/set", 33}, {"minecraft:serversettings/view_distance", 50},
				{"minecraft:serversettings/view_distance/set", 51}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getInputControlType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:boolean", 0}, {"minecraft:number_range", 1}, {"minecraft:single_option", 2}, {"minecraft:text", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getIntProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:biased_to_bottom", 2}, {"minecraft:clamped", 3}, {"minecraft:clamped_normal", 5}, {"minecraft:constant", 0},
				{"minecraft:uniform", 1}, {"minecraft:weighted_list", 4}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getItem() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:acacia_boat", 871}, {"minecraft:acacia_button", 755}, {"minecraft:acacia_chest_boat", 872}, {"minecraft:acacia_door", 784},
				{"minecraft:acacia_fence", 348}, {"minecraft:acacia_fence_gate", 825}, {"minecraft:acacia_hanging_sign", 1004}, {"minecraft:acacia_leaves", 186},
				{"minecraft:acacia_log", 138}, {"minecraft:acacia_planks", 40}, {"minecraft:acacia_pressure_plate", 771}, {"minecraft:acacia_sapling", 53},
				{"minecraft:acacia_shelf", 305}, {"minecraft:acacia_sign", 992}, {"minecraft:acacia_slab", 274}, {"minecraft:acacia_stairs", 445},
				{"minecraft:acacia_trapdoor", 805}, {"minecraft:acacia_wood", 175}, {"minecraft:activator_rail", 836}, {"minecraft:air", 0},
				{"minecraft:allay_spawn_egg", 1131}, {"minecraft:allium", 234}, {"minecraft:amethyst_block", 88}, {"minecraft:amethyst_cluster", 1401},
				{"minecraft:amethyst_shard", 902}, {"minecraft:ancient_debris", 82}, {"minecraft:andesite", 6}, {"minecraft:andesite_slab", 708},
				{"minecraft:andesite_stairs", 691}, {"minecraft:andesite_wall", 466}, {"minecraft:angler_pottery_sherd", 1428}, {"minecraft:anvil", 478},
				{"minecraft:apple", 893}, {"minecraft:archer_pottery_sherd", 1429}, {"minecraft:armadillo_scute", 889}, {"minecraft:armadillo_spawn_egg", 1130},
				{"minecraft:armor_stand", 1250}, {"minecraft:arms_up_pottery_sherd", 1430}, {"minecraft:arrow", 895}, {"minecraft:axolotl_bucket", 1023},
				{"minecraft:axolotl_spawn_egg", 1132}, {"minecraft:azalea", 205}, {"minecraft:azalea_leaves", 191}, {"minecraft:azure_bluet", 235},
				{"minecraft:baked_potato", 1225}, {"minecraft:bamboo", 269}, {"minecraft:bamboo_block", 147}, {"minecraft:bamboo_button", 760},
				{"minecraft:bamboo_chest_raft", 882}, {"minecraft:bamboo_door", 789}, {"minecraft:bamboo_fence", 353}, {"minecraft:bamboo_fence_gate", 830},
				{"minecraft:bamboo_hanging_sign", 1009}, {"minecraft:bamboo_mosaic", 48}, {"minecraft:bamboo_mosaic_slab", 280}, {"minecraft:bamboo_mosaic_stairs", 451},
				{"minecraft:bamboo_planks", 45}, {"minecraft:bamboo_pressure_plate", 776}, {"minecraft:bamboo_raft", 881}, {"minecraft:bamboo_shelf", 306},
				{"minecraft:bamboo_sign", 997}, {"minecraft:bamboo_slab", 279}, {"minecraft:bamboo_stairs", 450}, {"minecraft:bamboo_trapdoor", 810},
				{"minecraft:barrel", 1337}, {"minecraft:barrier", 502}, {"minecraft:basalt", 362}, {"minecraft:bat_spawn_egg", 1133},
				{"minecraft:beacon", 455}, {"minecraft:bedrock", 58}, {"minecraft:bee_nest", 1362}, {"minecraft:bee_spawn_egg", 1134},
				{"minecraft:beef", 1110}, {"minecraft:beehive", 1363}, {"minecraft:beetroot", 1282}, {"minecraft:beetroot_seeds", 1283},
				{"minecraft:beetroot_soup", 1284}, {"minecraft:bell", 1345}, {"minecraft:big_dripleaf", 267}, {"minecraft:birch_boat", 867},
				{"minecraft:birch_button", 753}, {"minecraft:birch_chest_boat", 868}, {"minecraft:birch_door", 782}, {"minecraft:birch_fence", 346},
				{"minecraft:birch_fence_gate", 823}, {"minecraft:birch_hanging_sign", 1002}, {"minecraft:birch_leaves", 184}, {"minecraft:birch_log", 136},
				{"minecraft:birch_planks", 38}, {"minecraft:birch_pressure_plate", 769}, {"minecraft:birch_sapling", 51}, {"minecraft:birch_shelf", 307},
				{"minecraft:birch_sign", 990}, {"minecraft:birch_slab", 272}, {"minecraft:birch_stairs", 443}, {"minecraft:birch_trapdoor", 803},
				{"minecraft:birch_wood", 173}, {"minecraft:black_banner", 1276}, {"minecraft:black_bed", 1101}, {"minecraft:black_bundle", 1052},
				{"minecraft:black_candle", 1397}, {"minecraft:black_carpet", 520}, {"minecraft:black_concrete", 629}, {"minecraft:black_concrete_powder", 645},
				{"minecraft:black_dye", 1081}, {"minecraft:black_glazed_terracotta", 613}, {"minecraft:black_harness", 853}, {"minecraft:black_shulker_box", 597},
				{"minecraft:black_stained_glass", 545}, {"minecraft:black_stained_glass_pane", 561}, {"minecraft:black_terracotta", 501}, {"minecraft:black_wool", 228},
				{"minecraft:blackstone", 1368}, {"minecraft:blackstone_slab", 1369}, {"minecraft:blackstone_stairs", 1370}, {"minecraft:blackstone_wall", 471},
				{"minecraft:blade_pottery_sherd", 1431}, {"minecraft:blast_furnace", 1339}, {"minecraft:blaze_powder", 1124}, {"minecraft:blaze_rod", 1116},
				{"minecraft:blaze_spawn_egg", 1135}, {"minecraft:blue_banner", 1272}, {"minecraft:blue_bed", 1097}, {"minecraft:blue_bundle", 1048},
				{"minecraft:blue_candle", 1393}, {"minecraft:blue_carpet", 516}, {"minecraft:blue_concrete", 625}, {"minecraft:blue_concrete_powder", 641},
				{"minecraft:blue_dye", 1077}, {"minecraft:blue_egg", 1032}, {"minecraft:blue_glazed_terracotta", 609}, {"minecraft:blue_harness", 849},
				{"minecraft:blue_ice", 679}, {"minecraft:blue_orchid", 233}, {"minecraft:blue_shulker_box", 593}, {"minecraft:blue_stained_glass", 541},
				{"minecraft:blue_stained_glass_pane", 557}, {"minecraft:blue_terracotta", 497}, {"minecraft:blue_wool", 224}, {"minecraft:bogged_spawn_egg", 1136},
				{"minecraft:bolt_armor_trim_smithing_template", 1427}, {"minecraft:bone", 1083}, {"minecraft:bone_block", 579}, {"minecraft:bone_meal", 1082},
				{"minecraft:book", 1029}, {"minecraft:bookshelf", 317}, {"minecraft:bordure_indented_banner_pattern", 1334}, {"minecraft:bow", 894},
				{"minecraft:bowl", 892}, {"minecraft:brain_coral", 660}, {"minecraft:brain_coral_block", 655}, {"minecraft:brain_coral_fan", 670},
				{"minecraft:bread", 953}, {"minecraft:breeze_rod", 1218}, {"minecraft:breeze_spawn_egg", 1137}, {"minecraft:brewer_pottery_sherd", 1432},
				{"minecraft:brewing_stand", 1126}, {"minecraft:brick", 1025}, {"minecraft:brick_slab", 289}, {"minecraft:brick_stairs", 419},
				{"minecraft:brick_wall", 458}, {"minecraft:bricks", 304}, {"minecraft:brown_banner", 1273}, {"minecraft:brown_bed", 1098},
				{"minecraft:brown_bundle", 1049}, {"minecraft:brown_candle", 1394}, {"minecraft:brown_carpet", 517}, {"minecraft:brown_concrete", 626},
				{"minecraft:brown_concrete_powder", 642}, {"minecraft:brown_dye", 1078}, {"minecraft:brown_egg", 1033}, {"minecraft:brown_glazed_terracotta", 610},
				{"minecraft:brown_harness", 850}, {"minecraft:brown_mushroom", 247}, {"minecraft:brown_mushroom_block", 387}, {"minecraft:brown_shulker_box", 594},
				{"minecraft:brown_stained_glass", 542}, {"minecraft:brown_stained_glass_pane", 558}, {"minecraft:brown_terracotta", 498}, {"minecraft:brown_wool", 225},
				{"minecraft:brush", 1408}, {"minecraft:bubble_coral", 661}, {"minecraft:bubble_coral_block", 656}, {"minecraft:bubble_coral_fan", 671},
				{"minecraft:bucket", 1012}, {"minecraft:budding_amethyst", 89}, {"minecraft:bundle", 1036}, {"minecraft:burn_pottery_sherd", 1433},
				{"minecraft:bush", 204}, {"minecraft:cactus", 340}, {"minecraft:cactus_flower", 341}, {"minecraft:cake", 1085},
				{"minecraft:calcite", 11}, {"minecraft:calibrated_sculk_sensor", 743}, {"minecraft:camel_spawn_egg", 1139}, {"minecraft:campfire", 1358},
				{"minecraft:candle", 1381}, {"minecraft:carrot", 1223}, {"minecraft:carrot_on_a_stick", 859}, {"minecraft:cartography_table", 1340},
				{"minecraft:carved_pumpkin", 357}, {"minecraft:cat_spawn_egg", 1138}, {"minecraft:cauldron", 1127}, {"minecraft:cave_spider_spawn_egg", 1140},
				{"minecraft:chain_command_block", 574}, {"minecraft:chainmail_boots", 965}, {"minecraft:chainmail_chestplate", 963}, {"minecraft:chainmail_helmet", 962},
				{"minecraft:chainmail_leggings", 964}, {"minecraft:charcoal", 897}, {"minecraft:cherry_boat", 873}, {"minecraft:cherry_button", 756},
				{"minecraft:cherry_chest_boat", 874}, {"minecraft:cherry_door", 785}, {"minecraft:cherry_fence", 349}, {"minecraft:cherry_fence_gate", 826},
				{"minecraft:cherry_hanging_sign", 1005}, {"minecraft:cherry_leaves", 187}, {"minecraft:cherry_log", 139}, {"minecraft:cherry_planks", 41},
				{"minecraft:cherry_pressure_plate", 772}, {"minecraft:cherry_sapling", 54}, {"minecraft:cherry_shelf", 308}, {"minecraft:cherry_sign", 993},
				{"minecraft:cherry_slab", 275}, {"minecraft:cherry_stairs", 446}, {"minecraft:cherry_trapdoor", 806}, {"minecraft:cherry_wood", 176},
				{"minecraft:chest", 331}, {"minecraft:chest_minecart", 855}, {"minecraft:chicken", 1112}, {"minecraft:chicken_spawn_egg", 1141},
				{"minecraft:chipped_anvil", 479}, {"minecraft:chiseled_bookshelf", 318}, {"minecraft:chiseled_copper", 98}, {"minecraft:chiseled_deepslate", 385},
				{"minecraft:chiseled_nether_bricks", 426}, {"minecraft:chiseled_polished_blackstone", 1375}, {"minecraft:chiseled_quartz_block", 481}, {"minecraft:chiseled_red_sandstone", 570},
				{"minecraft:chiseled_resin_bricks", 418}, {"minecraft:chiseled_sandstone", 199}, {"minecraft:chiseled_stone_bricks", 378}, {"minecraft:chiseled_tuff", 16},
				{"minecraft:chiseled_tuff_bricks", 25}, {"minecraft:chorus_flower", 325}, {"minecraft:chorus_fruit", 1278}, {"minecraft:chorus_plant", 324},
				{"minecraft:clay", 342}, {"minecraft:clay_ball", 1026}, {"minecraft:clock", 1054}, {"minecraft:closed_eyeblossom", 231},
				{"minecraft:coal", 896}, {"minecraft:coal_block", 83}, {"minecraft:coal_ore", 64}, {"minecraft:coarse_dirt", 29},
				{"minecraft:coast_armor_trim_smithing_template", 1412}, {"minecraft:cobbled_deepslate", 9}, {"minecraft:cobbled_deepslate_slab", 712}, {"minecraft:cobbled_deepslate_stairs", 695},
				{"minecraft:cobbled_deepslate_wall", 474}, {"minecraft:cobblestone", 35}, {"minecraft:cobblestone_slab", 288}, {"minecraft:cobblestone_stairs", 336},
				{"minecraft:cobblestone_wall", 456}, {"minecraft:cobweb", 201}, {"minecraft:cocoa_beans", 1065}, {"minecraft:cod", 1057},
				{"minecraft:cod_bucket", 1021}, {"minecraft:cod_spawn_egg", 1142}, {"minecraft:command_block", 454}, {"minecraft:command_block_minecart", 1258},
				{"minecraft:comparator", 721}, {"minecraft:compass", 1034}, {"minecraft:composter", 1336}, {"minecraft:conduit", 680},
				{"minecraft:cooked_beef", 1111}, {"minecraft:cooked_chicken", 1113}, {"minecraft:cooked_cod", 1061}, {"minecraft:cooked_mutton", 1260},
				{"minecraft:cooked_porkchop", 984}, {"minecraft:cooked_rabbit", 1246}, {"minecraft:cooked_salmon", 1062}, {"minecraft:cookie", 1102},
				{"minecraft:copper_axe", 919}, {"minecraft:copper_bars", 391}, {"minecraft:copper_block", 91}, {"minecraft:copper_boots", 961},
				{"minecraft:copper_bulb", 1459}, {"minecraft:copper_chain", 400}, {"minecraft:copper_chest", 1467}, {"minecraft:copper_chestplate", 959},
				{"minecraft:copper_door", 792}, {"minecraft:copper_golem_spawn_egg", 1143}, {"minecraft:copper_golem_statue", 1475}, {"minecraft:copper_grate", 1451},
				{"minecraft:copper_helmet", 958}, {"minecraft:copper_hoe", 920}, {"minecraft:copper_horse_armor", 1251}, {"minecraft:copper_ingot", 906},
				{"minecraft:copper_lantern", 1348}, {"minecraft:copper_leggings", 960}, {"minecraft:copper_nugget", 1294}, {"minecraft:copper_ore", 68},
				{"minecraft:copper_pickaxe", 918}, {"minecraft:copper_shovel", 917}, {"minecraft:copper_sword", 916}, {"minecraft:copper_torch", 366},
				{"minecraft:copper_trapdoor", 813}, {"minecraft:cornflower", 241}, {"minecraft:cow_spawn_egg", 1144}, {"minecraft:cracked_deepslate_bricks", 382},
				{"minecraft:cracked_deepslate_tiles", 384}, {"minecraft:cracked_nether_bricks", 425}, {"minecraft:cracked_polished_blackstone_bricks", 1379}, {"minecraft:cracked_stone_bricks", 377},
				{"minecraft:crafter", 1103}, {"minecraft:crafting_table", 332}, {"minecraft:creaking_heart", 330}, {"minecraft:creaking_spawn_egg", 1208},
				{"minecraft:creeper_banner_pattern", 1326}, {"minecraft:creeper_head", 1233}, {"minecraft:creeper_spawn_egg", 1145}, {"minecraft:crimson_button", 761},
				{"minecraft:crimson_door", 790}, {"minecraft:crimson_fence", 354}, {"minecraft:crimson_fence_gate", 831}, {"minecraft:crimson_fungus", 249},
				{"minecraft:crimson_hanging_sign", 1010}, {"minecraft:crimson_hyphae", 180}, {"minecraft:crimson_nylium", 33}, {"minecraft:crimson_planks", 46},
				{"minecraft:crimson_pressure_plate", 777}, {"minecraft:crimson_roots", 251}, {"minecraft:crimson_shelf", 309}, {"minecraft:crimson_sign", 998},
				{"minecraft:crimson_slab", 281}, {"minecraft:crimson_stairs", 452}, {"minecraft:crimson_stem", 145}, {"minecraft:crimson_trapdoor", 811},
				{"minecraft:crossbow", 1322}, {"minecraft:crying_obsidian", 1367}, {"minecraft:cut_copper", 102}, {"minecraft:cut_copper_slab", 110},
				{"minecraft:cut_copper_stairs", 106}, {"minecraft:cut_red_sandstone", 571}, {"minecraft:cut_red_sandstone_slab", 295}, {"minecraft:cut_sandstone", 200},
				{"minecraft:cut_sandstone_slab", 286}, {"minecraft:cyan_banner", 1270}, {"minecraft:cyan_bed", 1095}, {"minecraft:cyan_bundle", 1046},
				{"minecraft:cyan_candle", 1391}, {"minecraft:cyan_carpet", 514}, {"minecraft:cyan_concrete", 623}, {"minecraft:cyan_concrete_powder", 639},
				{"minecraft:cyan_dye", 1075}, {"minecraft:cyan_glazed_terracotta", 607}, {"minecraft:cyan_harness", 847}, {"minecraft:cyan_shulker_box", 591},
				{"minecraft:cyan_stained_glass", 539}, {"minecraft:cyan_stained_glass_pane", 555}, {"minecraft:cyan_terracotta", 495}, {"minecraft:cyan_wool", 222},
				{"minecraft:damaged_anvil", 480}, {"minecraft:dandelion", 229}, {"minecraft:danger_pottery_sherd", 1434}, {"minecraft:dark_oak_boat", 875},
				{"minecraft:dark_oak_button", 757}, {"minecraft:dark_oak_chest_boat", 876}, {"minecraft:dark_oak_door", 786}, {"minecraft:dark_oak_fence", 350},
				{"minecraft:dark_oak_fence_gate", 827}, {"minecraft:dark_oak_hanging_sign", 1006}, {"minecraft:dark_oak_leaves", 188}, {"minecraft:dark_oak_log", 141},
				{"minecraft:dark_oak_planks", 42}, {"minecraft:dark_oak_pressure_plate", 773}, {"minecraft:dark_oak_sapling", 55}, {"minecraft:dark_oak_shelf", 310},
				{"minecraft:dark_oak_sign", 994}, {"minecraft:dark_oak_slab", 276}, {"minecraft:dark_oak_stairs", 447}, {"minecraft:dark_oak_trapdoor", 807},
				{"minecraft:dark_oak_wood", 178}, {"minecraft:dark_prismarine", 564}, {"minecraft:dark_prismarine_slab", 299}, {"minecraft:dark_prismarine_stairs", 567},
				{"minecraft:daylight_detector", 741}, {"minecraft:dead_brain_coral", 664}, {"minecraft:dead_brain_coral_block", 650}, {"minecraft:dead_brain_coral_fan", 675},
				{"minecraft:dead_bubble_coral", 665}, {"minecraft:dead_bubble_coral_block", 651}, {"minecraft:dead_bubble_coral_fan", 676}, {"minecraft:dead_bush", 207},
				{"minecraft:dead_fire_coral", 666}, {"minecraft:dead_fire_coral_block", 652}, {"minecraft:dead_fire_coral_fan", 677}, {"minecraft:dead_horn_coral", 667},
				{"minecraft:dead_horn_coral_block", 653}, {"minecraft:dead_horn_coral_fan", 678}, {"minecraft:dead_tube_coral", 668}, {"minecraft:dead_tube_coral_block", 649},
				{"minecraft:dead_tube_coral_fan", 674}, {"minecraft:debug_stick", 1296}, {"minecraft:decorated_pot", 319}, {"minecraft:deepslate", 8},
				{"minecraft:deepslate_brick_slab", 714}, {"minecraft:deepslate_brick_stairs", 697}, {"minecraft:deepslate_brick_wall", 476}, {"minecraft:deepslate_bricks", 381},
				{"minecraft:deepslate_coal_ore", 65}, {"minecraft:deepslate_copper_ore", 69}, {"minecraft:deepslate_diamond_ore", 79}, {"minecraft:deepslate_emerald_ore", 75},
				{"minecraft:deepslate_gold_ore", 71}, {"minecraft:deepslate_iron_ore", 67}, {"minecraft:deepslate_lapis_ore", 77}, {"minecraft:deepslate_redstone_ore", 73},
				{"minecraft:deepslate_tile_slab", 715}, {"minecraft:deepslate_tile_stairs", 698}, {"minecraft:deepslate_tile_wall", 477}, {"minecraft:deepslate_tiles", 383},
				{"minecraft:detector_rail", 834}, {"minecraft:diamond", 898}, {"minecraft:diamond_axe", 939}, {"minecraft:diamond_block", 93},
				{"minecraft:diamond_boots", 973}, {"minecraft:diamond_chestplate", 971}, {"minecraft:diamond_helmet", 970}, {"minecraft:diamond_hoe", 940},
				{"minecraft:diamond_horse_armor", 1254}, {"minecraft:diamond_leggings", 972}, {"minecraft:diamond_ore", 78}, {"minecraft:diamond_pickaxe", 938},
				{"minecraft:diamond_shovel", 937}, {"minecraft:diamond_sword", 936}, {"minecraft:diorite", 4}, {"minecraft:diorite_slab", 711},
				{"minecraft:diorite_stairs", 694}, {"minecraft:diorite_wall", 470}, {"minecraft:dirt", 28}, {"minecraft:dirt_path", 523},
				{"minecraft:disc_fragment_5", 1318}, {"minecraft:dispenser", 728}, {"minecraft:dolphin_spawn_egg", 1146}, {"minecraft:donkey_spawn_egg", 1147},
				{"minecraft:dragon_breath", 1285}, {"minecraft:dragon_egg", 437}, {"minecraft:dragon_head", 1234}, {"minecraft:dried_ghast", 648},
				{"minecraft:dried_kelp", 1107}, {"minecraft:dried_kelp_block", 1027}, {"minecraft:dripstone_block", 26}, {"minecraft:dropper", 729},
				{"minecraft:drowned_spawn_egg", 1148}, {"minecraft:dune_armor_trim_smithing_template", 1411}, {"minecraft:echo_shard", 1407}, {"minecraft:egg", 1031},
				{"minecraft:elder_guardian_spawn_egg", 1149}, {"minecraft:elytra", 862}, {"minecraft:emerald", 899}, {"minecraft:emerald_block", 440},
				{"minecraft:emerald_ore", 74}, {"minecraft:enchanted_book", 1240}, {"minecraft:enchanted_golden_apple", 987}, {"minecraft:enchanting_table", 433},
				{"minecraft:end_crystal", 1277}, {"minecraft:end_portal_frame", 434}, {"minecraft:end_rod", 323}, {"minecraft:end_stone", 435},
				{"minecraft:end_stone_brick_slab", 704}, {"minecraft:end_stone_brick_stairs", 686}, {"minecraft:end_stone_brick_wall", 469}, {"minecraft:end_stone_bricks", 436},
				{"minecraft:ender_chest", 439}, {"minecraft:ender_dragon_spawn_egg", 1150}, {"minecraft:ender_eye", 1128}, {"minecraft:ender_pearl", 1115},
				{"minecraft:enderman_spawn_egg", 1151}, {"minecraft:endermite_spawn_egg", 1152}, {"minecraft:evoker_spawn_egg", 1153}, {"minecraft:experience_bottle", 1213},
				{"minecraft:explorer_pottery_sherd", 1435}, {"minecraft:exposed_chiseled_copper", 99}, {"minecraft:exposed_copper", 95}, {"minecraft:exposed_copper_bars", 392},
				{"minecraft:exposed_copper_bulb", 1460}, {"minecraft:exposed_copper_chain", 401}, {"minecraft:exposed_copper_chest", 1468}, {"minecraft:exposed_copper_door", 793},
				{"minecraft:exposed_copper_golem_statue", 1476}, {"minecraft:exposed_copper_grate", 1452}, {"minecraft:exposed_copper_lantern", 1349}, {"minecraft:exposed_copper_trapdoor", 814},
				{"minecraft:exposed_cut_copper", 103}, {"minecraft:exposed_cut_copper_slab", 111}, {"minecraft:exposed_cut_copper_stairs", 107}, {"minecraft:exposed_lightning_rod", 734},
				{"minecraft:eye_armor_trim_smithing_template", 1415}, {"minecraft:farmland", 333}, {"minecraft:feather", 949}, {"minecraft:fermented_spider_eye", 1123},
				{"minecraft:fern", 203}, {"minecraft:field_masoned_banner_pattern", 1333}, {"minecraft:filled_map", 1104}, {"minecraft:fire_charge", 1214},
				{"minecraft:fire_coral", 662}, {"minecraft:fire_coral_block", 657}, {"minecraft:fire_coral_fan", 672}, {"minecraft:firefly_bush", 208},
				{"minecraft:firework_rocket", 1238}, {"minecraft:firework_star", 1239}, {"minecraft:fishing_rod", 1053}, {"minecraft:fletching_table", 1341},
				{"minecraft:flint", 982}, {"minecraft:flint_and_steel", 891}, {"minecraft:flow_armor_trim_smithing_template", 1426}, {"minecraft:flow_banner_pattern", 1331},
				{"minecraft:flow_pottery_sherd", 1436}, {"minecraft:flower_banner_pattern", 1325}, {"minecraft:flower_pot", 1222}, {"minecraft:flowering_azalea", 206},
				{"minecraft:flowering_azalea_leaves", 192}, {"minecraft:fox_spawn_egg", 1154}, {"minecraft:friend_pottery_sherd", 1437}, {"minecraft:frog_spawn_egg", 1155},
				{"minecraft:frogspawn", 1406}, {"minecraft:furnace", 334}, {"minecraft:furnace_minecart", 856}, {"minecraft:ghast_spawn_egg", 1156},
				{"minecraft:ghast_tear", 1117}, {"minecraft:gilded_blackstone", 1371}, {"minecraft:glass", 195}, {"minecraft:glass_bottle", 1120},
				{"minecraft:glass_pane", 408}, {"minecraft:glistering_melon_slice", 1129}, {"minecraft:globe_banner_pattern", 1329}, {"minecraft:glow_berries", 1357},
				{"minecraft:glow_ink_sac", 1064}, {"minecraft:glow_item_frame", 1221}, {"minecraft:glow_lichen", 411}, {"minecraft:glow_squid_spawn_egg", 1158},
				{"minecraft:glowstone", 367}, {"minecraft:glowstone_dust", 1056}, {"minecraft:goat_horn", 1335}, {"minecraft:goat_spawn_egg", 1159},
				{"minecraft:gold_block", 92}, {"minecraft:gold_ingot", 908}, {"minecraft:gold_nugget", 1118}, {"minecraft:gold_ore", 70},
				{"minecraft:golden_apple", 986}, {"minecraft:golden_axe", 929}, {"minecraft:golden_boots", 977}, {"minecraft:golden_carrot", 1228},
				{"minecraft:golden_chestplate", 975}, {"minecraft:golden_helmet", 974}, {"minecraft:golden_hoe", 930}, {"minecraft:golden_horse_armor", 1253},
				{"minecraft:golden_leggings", 976}, {"minecraft:golden_pickaxe", 928}, {"minecraft:golden_shovel", 927}, {"minecraft:golden_sword", 926},
				{"minecraft:granite", 2}, {"minecraft:granite_slab", 707}, {"minecraft:granite_stairs", 690}, {"minecraft:granite_wall", 462},
				{"minecraft:grass_block", 27}, {"minecraft:gravel", 63}, {"minecraft:gray_banner", 1268}, {"minecraft:gray_bed", 1093},
				{"minecraft:gray_bundle", 1044}, {"minecraft:gray_candle", 1389}, {"minecraft:gray_carpet", 512}, {"minecraft:gray_concrete", 621},
				{"minecraft:gray_concrete_powder", 637}, {"minecraft:gray_dye", 1073}, {"minecraft:gray_glazed_terracotta", 605}, {"minecraft:gray_harness", 845},
				{"minecraft:gray_shulker_box", 589}, {"minecraft:gray_stained_glass", 537}, {"minecraft:gray_stained_glass_pane", 553}, {"minecraft:gray_terracotta", 493},
				{"minecraft:gray_wool", 220}, {"minecraft:green_banner", 1274}, {"minecraft:green_bed", 1099}, {"minecraft:green_bundle", 1050},
				{"minecraft:green_candle", 1395}, {"minecraft:green_carpet", 518}, {"minecraft:green_concrete", 627}, {"minecraft:green_concrete_powder", 643},
				{"minecraft:green_dye", 1079}, {"minecraft:green_glazed_terracotta", 611}, {"minecraft:green_harness", 851}, {"minecraft:green_shulker_box", 595},
				{"minecraft:green_stained_glass", 543}, {"minecraft:green_stained_glass_pane", 559}, {"minecraft:green_terracotta", 499}, {"minecraft:green_wool", 226},
				{"minecraft:grindstone", 1342}, {"minecraft:guardian_spawn_egg", 1160}, {"minecraft:gunpowder", 950}, {"minecraft:guster_banner_pattern", 1332},
				{"minecraft:guster_pottery_sherd", 1438}, {"minecraft:hanging_roots", 266}, {"minecraft:happy_ghast_spawn_egg", 1157}, {"minecraft:hay_block", 504},
				{"minecraft:heart_of_the_sea", 1321}, {"minecraft:heart_pottery_sherd", 1439}, {"minecraft:heartbreak_pottery_sherd", 1440}, {"minecraft:heavy_core", 87},
				{"minecraft:heavy_weighted_pressure_plate", 766}, {"minecraft:hoglin_spawn_egg", 1161}, {"minecraft:honey_block", 725}, {"minecraft:honey_bottle", 1364},
				{"minecraft:honeycomb", 1361}, {"minecraft:honeycomb_block", 1365}, {"minecraft:hopper", 727}, {"minecraft:hopper_minecart", 858},
				{"minecraft:horn_coral", 663}, {"minecraft:horn_coral_block", 658}, {"minecraft:horn_coral_fan", 673}, {"minecraft:horse_spawn_egg", 1162},
				{"minecraft:host_armor_trim_smithing_template", 1425}, {"minecraft:howl_pottery_sherd", 1441}, {"minecraft:husk_spawn_egg", 1163}, {"minecraft:ice", 338},
				{"minecraft:infested_chiseled_stone_bricks", 373}, {"minecraft:infested_cobblestone", 369}, {"minecraft:infested_cracked_stone_bricks", 372}, {"minecraft:infested_deepslate", 374},
				{"minecraft:infested_mossy_stone_bricks", 371}, {"minecraft:infested_stone", 368}, {"minecraft:infested_stone_bricks", 370}, {"minecraft:ink_sac", 1063},
				{"minecraft:iron_axe", 934}, {"minecraft:iron_bars", 390}, {"minecraft:iron_block", 90}, {"minecraft:iron_boots", 969},
				{"minecraft:iron_chain", 399}, {"minecraft:iron_chestplate", 967}, {"minecraft:iron_door", 779}, {"minecraft:iron_golem_spawn_egg", 1164},
				{"minecraft:iron_helmet", 966}, {"minecraft:iron_hoe", 935}, {"minecraft:iron_horse_armor", 1252}, {"minecraft:iron_ingot", 904},
				{"minecraft:iron_leggings", 968}, {"minecraft:iron_nugget", 1293}, {"minecraft:iron_ore", 66}, {"minecraft:iron_pickaxe", 933},
				{"minecraft:iron_shovel", 932}, {"minecraft:iron_sword", 931}, {"minecraft:iron_trapdoor", 800}, {"minecraft:item_frame", 1220},
				{"minecraft:jack_o_lantern", 358}, {"minecraft:jigsaw", 884}, {"minecraft:jukebox", 343}, {"minecraft:jungle_boat", 869},
				{"minecraft:jungle_button", 754}, {"minecraft:jungle_chest_boat", 870}, {"minecraft:jungle_door", 783}, {"minecraft:jungle_fence", 347},
				{"minecraft:jungle_fence_gate", 824}, {"minecraft:jungle_hanging_sign", 1003}, {"minecraft:jungle_leaves", 185}, {"minecraft:jungle_log", 137},
				{"minecraft:jungle_planks", 39}, {"minecraft:jungle_pressure_plate", 770}, {"minecraft:jungle_sapling", 52}, {"minecraft:jungle_shelf", 311},
				{"minecraft:jungle_sign", 991}, {"minecraft:jungle_slab", 273}, {"minecraft:jungle_stairs", 444}, {"minecraft:jungle_trapdoor", 804},
				{"minecraft:jungle_wood", 174}, {"minecraft:kelp", 257}, {"minecraft:knowledge_book", 1295}, {"minecraft:ladder", 335},
				{"minecraft:lantern", 1346}, {"minecraft:lapis_block", 197}, {"minecraft:lapis_lazuli", 900}, {"minecraft:lapis_ore", 76},
				{"minecraft:large_amethyst_bud", 1400}, {"minecraft:large_fern", 529}, {"minecraft:lava_bucket", 1014}, {"minecraft:lead", 1256},
				{"minecraft:leaf_litter", 260}, {"minecraft:leather", 1017}, {"minecraft:leather_boots", 957}, {"minecraft:leather_chestplate", 955},
				{"minecraft:leather_helmet", 954}, {"minecraft:leather_horse_armor", 1255}, {"minecraft:leather_leggings", 956}, {"minecraft:lectern", 730},
				{"minecraft:lever", 732}, {"minecraft:light", 503}, {"minecraft:light_blue_banner", 1264}, {"minecraft:light_blue_bed", 1089},
				{"minecraft:light_blue_bundle", 1040}, {"minecraft:light_blue_candle", 1385}, {"minecraft:light_blue_carpet", 508}, {"minecraft:light_blue_concrete", 617},
				{"minecraft:light_blue_concrete_powder", 633}, {"minecraft:light_blue_dye", 1069}, {"minecraft:light_blue_glazed_terracotta", 601}, {"minecraft:light_blue_harness", 841},
				{"minecraft:light_blue_shulker_box", 585}, {"minecraft:light_blue_stained_glass", 533}, {"minecraft:light_blue_stained_glass_pane", 549}, {"minecraft:light_blue_terracotta", 489},
				{"minecraft:light_blue_wool", 216}, {"minecraft:light_gray_banner", 1269}, {"minecraft:light_gray_bed", 1094}, {"minecraft:light_gray_bundle", 1045},
				{"minecraft:light_gray_candle", 1390}, {"minecraft:light_gray_carpet", 513}, {"minecraft:light_gray_concrete", 622}, {"minecraft:light_gray_concrete_powder", 638},
				{"minecraft:light_gray_dye", 1074}, {"minecraft:light_gray_glazed_terracotta", 606}, {"minecraft:light_gray_harness", 846}, {"minecraft:light_gray_shulker_box", 590},
				{"minecraft:light_gray_stained_glass", 538}, {"minecraft:light_gray_stained_glass_pane", 554}, {"minecraft:light_gray_terracotta", 494}, {"minecraft:light_gray_wool", 221},
				{"minecraft:light_weighted_pressure_plate", 765}, {"minecraft:lightning_rod", 733}, {"minecraft:lilac", 525}, {"minecraft:lily_of_the_valley", 242},
				{"minecraft:lily_pad", 423}, {"minecraft:lime_banner", 1266}, {"minecraft:lime_bed", 1091}, {"minecraft:lime_bundle", 1042},
				{"minecraft:lime_candle", 1387}, {"minecraft:lime_carpet", 510}, {"minecraft:lime_concrete", 619}, {"minecraft:lime_concrete_powder", 635},
				{"minecraft:lime_dye", 1071}, {"minecraft:lime_glazed_terracotta", 603}, {"minecraft:lime_harness", 843}, {"minecraft:lime_shulker_box", 587},
				{"minecraft:lime_stained_glass", 535}, {"minecraft:lime_stained_glass_pane", 551}, {"minecraft:lime_terracotta", 491}, {"minecraft:lime_wool", 218},
				{"minecraft:lingering_potion", 1289}, {"minecraft:llama_spawn_egg", 1165}, {"minecraft:lodestone", 1366}, {"minecraft:loom", 1324},
				{"minecraft:mace", 1219}, {"minecraft:magenta_banner", 1263}, {"minecraft:magenta_bed", 1088}, {"minecraft:magenta_bundle", 1039},
				{"minecraft:magenta_candle", 1384}, {"minecraft:magenta_carpet", 507}, {"minecraft:magenta_concrete", 616}, {"minecraft:magenta_concrete_powder", 632},
				{"minecraft:magenta_dye", 1068}, {"minecraft:magenta_glazed_terracotta", 600}, {"minecraft:magenta_harness", 840}, {"minecraft:magenta_shulker_box", 584},
				{"minecraft:magenta_stained_glass", 532}, {"minecraft:magenta_stained_glass_pane", 548}, {"minecraft:magenta_terracotta", 488}, {"minecraft:magenta_wool", 215},
				{"minecraft:magma_block", 575}, {"minecraft:magma_cream", 1125}, {"minecraft:magma_cube_spawn_egg", 1166}, {"minecraft:mangrove_boat", 879},
				{"minecraft:mangrove_button", 759}, {"minecraft:mangrove_chest_boat", 880}, {"minecraft:mangrove_door", 788}, {"minecraft:mangrove_fence", 352},
				{"minecraft:mangrove_fence_gate", 829}, {"minecraft:mangrove_hanging_sign", 1008}, {"minecraft:mangrove_leaves", 190}, {"minecraft:mangrove_log", 142},
				{"minecraft:mangrove_planks", 44}, {"minecraft:mangrove_pressure_plate", 775}, {"minecraft:mangrove_propagule", 57}, {"minecraft:mangrove_roots", 143},
				{"minecraft:mangrove_shelf", 312}, {"minecraft:mangrove_sign", 996}, {"minecraft:mangrove_slab", 278}, {"minecraft:mangrove_stairs", 449},
				{"minecraft:mangrove_trapdoor", 809}, {"minecraft:mangrove_wood", 179}, {"minecraft:map", 1227}, {"minecraft:medium_amethyst_bud", 1399},
				{"minecraft:melon", 409}, {"minecraft:melon_seeds", 1109}, {"minecraft:melon_slice", 1106}, {"minecraft:milk_bucket", 1018},
				{"minecraft:minecart", 854}, {"minecraft:miner_pottery_sherd", 1442}, {"minecraft:mojang_banner_pattern", 1328}, {"minecraft:mooshroom_spawn_egg", 1167},
				{"minecraft:moss_block", 262}, {"minecraft:moss_carpet", 261}, {"minecraft:mossy_cobblestone", 320}, {"minecraft:mossy_cobblestone_slab", 703},
				{"minecraft:mossy_cobblestone_stairs", 685}, {"minecraft:mossy_cobblestone_wall", 457}, {"minecraft:mossy_stone_brick_slab", 701}, {"minecraft:mossy_stone_brick_stairs", 683},
				{"minecraft:mossy_stone_brick_wall", 461}, {"minecraft:mossy_stone_bricks", 376}, {"minecraft:mourner_pottery_sherd", 1443}, {"minecraft:mud", 32},
				{"minecraft:mud_brick_slab", 291}, {"minecraft:mud_brick_stairs", 421}, {"minecraft:mud_brick_wall", 464}, {"minecraft:mud_bricks", 380},
				{"minecraft:muddy_mangrove_roots", 144}, {"minecraft:mule_spawn_egg", 1168}, {"minecraft:mushroom_stem", 389}, {"minecraft:mushroom_stew", 947},
				{"minecraft:music_disc_11", 1310}, {"minecraft:music_disc_13", 1297}, {"minecraft:music_disc_5", 1314}, {"minecraft:music_disc_blocks", 1299},
				{"minecraft:music_disc_cat", 1298}, {"minecraft:music_disc_chirp", 1300}, {"minecraft:music_disc_creator", 1301}, {"minecraft:music_disc_creator_music_box", 1302},
				{"minecraft:music_disc_far", 1303}, {"minecraft:music_disc_lava_chicken", 1304}, {"minecraft:music_disc_mall", 1305}, {"minecraft:music_disc_mellohi", 1306},
				{"minecraft:music_disc_otherside", 1312}, {"minecraft:music_disc_pigstep", 1315}, {"minecraft:music_disc_precipice", 1316}, {"minecraft:music_disc_relic", 1313},
				{"minecraft:music_disc_stal", 1307}, {"minecraft:music_disc_strad", 1308}, {"minecraft:music_disc_tears", 1317}, {"minecraft:music_disc_wait", 1311},
				{"minecraft:music_disc_ward", 1309}, {"minecraft:mutton", 1259}, {"minecraft:mycelium", 422}, {"minecraft:name_tag", 1257},
				{"minecraft:nautilus_shell", 1320}, {"minecraft:nether_brick", 1241}, {"minecraft:nether_brick_fence", 427}, {"minecraft:nether_brick_slab", 292},
				{"minecraft:nether_brick_stairs", 428}, {"minecraft:nether_brick_wall", 465}, {"minecraft:nether_bricks", 424}, {"minecraft:nether_gold_ore", 80},
				{"minecraft:nether_quartz_ore", 81}, {"minecraft:nether_sprouts", 253}, {"minecraft:nether_star", 1236}, {"minecraft:nether_wart", 1119},
				{"minecraft:nether_wart_block", 576}, {"minecraft:netherite_axe", 944}, {"minecraft:netherite_block", 94}, {"minecraft:netherite_boots", 981},
				{"minecraft:netherite_chestplate", 979}, {"minecraft:netherite_helmet", 978}, {"minecraft:netherite_hoe", 945}, {"minecraft:netherite_ingot", 909},
				{"minecraft:netherite_leggings", 980}, {"minecraft:netherite_pickaxe", 943}, {"minecraft:netherite_scrap", 910}, {"minecraft:netherite_shovel", 942},
				{"minecraft:netherite_sword", 941}, {"minecraft:netherite_upgrade_smithing_template", 1409}, {"minecraft:netherrack", 359}, {"minecraft:note_block", 748},
				{"minecraft:oak_boat", 863}, {"minecraft:oak_button", 751}, {"minecraft:oak_chest_boat", 864}, {"minecraft:oak_door", 780},
				{"minecraft:oak_fence", 344}, {"minecraft:oak_fence_gate", 821}, {"minecraft:oak_hanging_sign", 1000}, {"minecraft:oak_leaves", 182},
				{"minecraft:oak_log", 134}, {"minecraft:oak_planks", 36}, {"minecraft:oak_pressure_plate", 767}, {"minecraft:oak_sapling", 49},
				{"minecraft:oak_shelf", 313}, {"minecraft:oak_sign", 988}, {"minecraft:oak_slab", 270}, {"minecraft:oak_stairs", 441},
				{"minecraft:oak_trapdoor", 801}, {"minecraft:oak_wood", 171}, {"minecraft:observer", 726}, {"minecraft:obsidian", 321},
				{"minecraft:ocelot_spawn_egg", 1169}, {"minecraft:ochre_froglight", 1403}, {"minecraft:ominous_bottle", 1487}, {"minecraft:ominous_trial_key", 1485},
				{"minecraft:open_eyeblossom", 230}, {"minecraft:orange_banner", 1262}, {"minecraft:orange_bed", 1087}, {"minecraft:orange_bundle", 1038},
				{"minecraft:orange_candle", 1383}, {"minecraft:orange_carpet", 506}, {"minecraft:orange_concrete", 615}, {"minecraft:orange_concrete_powder", 631},
				{"minecraft:orange_dye", 1067}, {"minecraft:orange_glazed_terracotta", 599}, {"minecraft:orange_harness", 839}, {"minecraft:orange_shulker_box", 583},
				{"minecraft:orange_stained_glass", 531}, {"minecraft:orange_stained_glass_pane", 547}, {"minecraft:orange_terracotta", 487}, {"minecraft:orange_tulip", 237},
				{"minecraft:orange_wool", 214}, {"minecraft:oxeye_daisy", 240}, {"minecraft:oxidized_chiseled_copper", 101}, {"minecraft:oxidized_copper", 97},
				{"minecraft:oxidized_copper_bars", 394}, {"minecraft:oxidized_copper_bulb", 1462}, {"minecraft:oxidized_copper_chain", 403}, {"minecraft:oxidized_copper_chest", 1470},
				{"minecraft:oxidized_copper_door", 795}, {"minecraft:oxidized_copper_golem_statue", 1478}, {"minecraft:oxidized_copper_grate", 1454}, {"minecraft:oxidized_copper_lantern", 1351},
				{"minecraft:oxidized_copper_trapdoor", 816}, {"minecraft:oxidized_cut_copper", 105}, {"minecraft:oxidized_cut_copper_slab", 113}, {"minecraft:oxidized_cut_copper_stairs", 109},
				{"minecraft:oxidized_lightning_rod", 736}, {"minecraft:packed_ice", 522}, {"minecraft:packed_mud", 379}, {"minecraft:painting", 985},
				{"minecraft:pale_hanging_moss", 264}, {"minecraft:pale_moss_block", 265}, {"minecraft:pale_moss_carpet", 263}, {"minecraft:pale_oak_boat", 877},
				{"minecraft:pale_oak_button", 758}, {"minecraft:pale_oak_chest_boat", 878}, {"minecraft:pale_oak_door", 787}, {"minecraft:pale_oak_fence", 351},
				{"minecraft:pale_oak_fence_gate", 828}, {"minecraft:pale_oak_hanging_sign", 1007}, {"minecraft:pale_oak_leaves", 189}, {"minecraft:pale_oak_log", 140},
				{"minecraft:pale_oak_planks", 43}, {"minecraft:pale_oak_pressure_plate", 774}, {"minecraft:pale_oak_sapling", 56}, {"minecraft:pale_oak_shelf", 314},
				{"minecraft:pale_oak_sign", 995}, {"minecraft:pale_oak_slab", 277}, {"minecraft:pale_oak_stairs", 448}, {"minecraft:pale_oak_trapdoor", 808},
				{"minecraft:pale_oak_wood", 177}, {"minecraft:panda_spawn_egg", 1170}, {"minecraft:paper", 1028}, {"minecraft:parrot_spawn_egg", 1171},
				{"minecraft:pearlescent_froglight", 1405}, {"minecraft:peony", 527}, {"minecraft:petrified_oak_slab", 287}, {"minecraft:phantom_membrane", 861},
				{"minecraft:phantom_spawn_egg", 1172}, {"minecraft:pig_spawn_egg", 1173}, {"minecraft:piglin_banner_pattern", 1330}, {"minecraft:piglin_brute_spawn_egg", 1175},
				{"minecraft:piglin_head", 1235}, {"minecraft:piglin_spawn_egg", 1174}, {"minecraft:pillager_spawn_egg", 1176}, {"minecraft:pink_banner", 1267},
				{"minecraft:pink_bed", 1092}, {"minecraft:pink_bundle", 1043}, {"minecraft:pink_candle", 1388}, {"minecraft:pink_carpet", 511},
				{"minecraft:pink_concrete", 620}, {"minecraft:pink_concrete_powder", 636}, {"minecraft:pink_dye", 1072}, {"minecraft:pink_glazed_terracotta", 604},
				{"minecraft:pink_harness", 844}, {"minecraft:pink_petals", 258}, {"minecraft:pink_shulker_box", 588}, {"minecraft:pink_stained_glass", 536},
				{"minecraft:pink_stained_glass_pane", 552}, {"minecraft:pink_terracotta", 492}, {"minecraft:pink_tulip", 239}, {"minecraft:pink_wool", 219},
				{"minecraft:piston", 722}, {"minecraft:pitcher_plant", 245}, {"minecraft:pitcher_pod", 1281}, {"minecraft:player_head", 1231},
				{"minecraft:plenty_pottery_sherd", 1444}, {"minecraft:podzol", 30}, {"minecraft:pointed_dripstone", 1402}, {"minecraft:poisonous_potato", 1226},
				{"minecraft:polar_bear_spawn_egg", 1177}, {"minecraft:polished_andesite", 7}, {"minecraft:polished_andesite_slab", 710}, {"minecraft:polished_andesite_stairs", 693},
				{"minecraft:polished_basalt", 363}, {"minecraft:polished_blackstone", 1372}, {"minecraft:polished_blackstone_brick_slab", 1377}, {"minecraft:polished_blackstone_brick_stairs", 1378},
				{"minecraft:polished_blackstone_brick_wall", 473}, {"minecraft:polished_blackstone_bricks", 1376}, {"minecraft:polished_blackstone_button", 750}, {"minecraft:polished_blackstone_pressure_plate", 764},
				{"minecraft:polished_blackstone_slab", 1373}, {"minecraft:polished_blackstone_stairs", 1374}, {"minecraft:polished_blackstone_wall", 472}, {"minecraft:polished_deepslate", 10},
				{"minecraft:polished_deepslate_slab", 713}, {"minecraft:polished_deepslate_stairs", 696}, {"minecraft:polished_deepslate_wall", 475}, {"minecraft:polished_diorite", 5},
				{"minecraft:polished_diorite_slab", 702}, {"minecraft:polished_diorite_stairs", 684}, {"minecraft:polished_granite", 3}, {"minecraft:polished_granite_slab", 699},
				{"minecraft:polished_granite_stairs", 681}, {"minecraft:polished_tuff", 17}, {"minecraft:polished_tuff_slab", 18}, {"minecraft:polished_tuff_stairs", 19},
				{"minecraft:polished_tuff_wall", 20}, {"minecraft:popped_chorus_fruit", 1279}, {"minecraft:poppy", 232}, {"minecraft:porkchop", 983},
				{"minecraft:potato", 1224}, {"minecraft:potion", 1121}, {"minecraft:powder_snow_bucket", 1015}, {"minecraft:powered_rail", 833},
				{"minecraft:prismarine", 562}, {"minecraft:prismarine_brick_slab", 298}, {"minecraft:prismarine_brick_stairs", 566}, {"minecraft:prismarine_bricks", 563},
				{"minecraft:prismarine_crystals", 1244}, {"minecraft:prismarine_shard", 1243}, {"minecraft:prismarine_slab", 297}, {"minecraft:prismarine_stairs", 565},
				{"minecraft:prismarine_wall", 459}, {"minecraft:prize_pottery_sherd", 1445}, {"minecraft:pufferfish", 1060}, {"minecraft:pufferfish_bucket", 1019},
				{"minecraft:pufferfish_spawn_egg", 1178}, {"minecraft:pumpkin", 356}, {"minecraft:pumpkin_pie", 1237}, {"minecraft:pumpkin_seeds", 1108},
				{"minecraft:purple_banner", 1271}, {"minecraft:purple_bed", 1096}, {"minecraft:purple_bundle", 1047}, {"minecraft:purple_candle", 1392},
				{"minecraft:purple_carpet", 515}, {"minecraft:purple_concrete", 624}, {"minecraft:purple_concrete_powder", 640}, {"minecraft:purple_dye", 1076},
				{"minecraft:purple_glazed_terracotta", 608}, {"minecraft:purple_harness", 848}, {"minecraft:purple_shulker_box", 592}, {"minecraft:purple_stained_glass", 540},
				{"minecraft:purple_stained_glass_pane", 556}, {"minecraft:purple_terracotta", 496}, {"minecraft:purple_wool", 223}, {"minecraft:purpur_block", 326},
				{"minecraft:purpur_pillar", 327}, {"minecraft:purpur_slab", 296}, {"minecraft:purpur_stairs", 328}, {"minecraft:quartz", 901},
				{"minecraft:quartz_block", 482}, {"minecraft:quartz_bricks", 483}, {"minecraft:quartz_pillar", 484}, {"minecraft:quartz_slab", 293},
				{"minecraft:quartz_stairs", 485}, {"minecraft:rabbit", 1245}, {"minecraft:rabbit_foot", 1248}, {"minecraft:rabbit_hide", 1249},
				{"minecraft:rabbit_spawn_egg", 1179}, {"minecraft:rabbit_stew", 1247}, {"minecraft:rail", 835}, {"minecraft:raiser_armor_trim_smithing_template", 1424},
				{"minecraft:ravager_spawn_egg", 1180}, {"minecraft:raw_copper", 905}, {"minecraft:raw_copper_block", 85}, {"minecraft:raw_gold", 907},
				{"minecraft:raw_gold_block", 86}, {"minecraft:raw_iron", 903}, {"minecraft:raw_iron_block", 84}, {"minecraft:recovery_compass", 1035},
				{"minecraft:red_banner", 1275}, {"minecraft:red_bed", 1100}, {"minecraft:red_bundle", 1051}, {"minecraft:red_candle", 1396},
				{"minecraft:red_carpet", 519}, {"minecraft:red_concrete", 628}, {"minecraft:red_concrete_powder", 644}, {"minecraft:red_dye", 1080},
				{"minecraft:red_glazed_terracotta", 612}, {"minecraft:red_harness", 852}, {"minecraft:red_mushroom", 248}, {"minecraft:red_mushroom_block", 388},
				{"minecraft:red_nether_brick_slab", 709}, {"minecraft:red_nether_brick_stairs", 692}, {"minecraft:red_nether_brick_wall", 467}, {"minecraft:red_nether_bricks", 578},
				{"minecraft:red_sand", 62}, {"minecraft:red_sandstone", 569}, {"minecraft:red_sandstone_slab", 294}, {"minecraft:red_sandstone_stairs", 572},
				{"minecraft:red_sandstone_wall", 460}, {"minecraft:red_shulker_box", 596}, {"minecraft:red_stained_glass", 544}, {"minecraft:red_stained_glass_pane", 560},
				{"minecraft:red_terracotta", 500}, {"minecraft:red_tulip", 236}, {"minecraft:red_wool", 227}, {"minecraft:redstone", 717},
				{"minecraft:redstone_block", 719}, {"minecraft:redstone_lamp", 747}, {"minecraft:redstone_ore", 72}, {"minecraft:redstone_torch", 718},
				{"minecraft:reinforced_deepslate", 386}, {"minecraft:repeater", 720}, {"minecraft:repeating_command_block", 573}, {"minecraft:resin_block", 413},
				{"minecraft:resin_brick", 1242}, {"minecraft:resin_brick_slab", 416}, {"minecraft:resin_brick_stairs", 415}, {"minecraft:resin_brick_wall", 417},
				{"minecraft:resin_bricks", 414}, {"minecraft:resin_clump", 412}, {"minecraft:respawn_anchor", 1380}, {"minecraft:rib_armor_trim_smithing_template", 1419},
				{"minecraft:rooted_dirt", 31}, {"minecraft:rose_bush", 526}, {"minecraft:rotten_flesh", 1114}, {"minecraft:saddle", 837},
				{"minecraft:salmon", 1058}, {"minecraft:salmon_bucket", 1020}, {"minecraft:salmon_spawn_egg", 1181}, {"minecraft:sand", 59},
				{"minecraft:sandstone", 198}, {"minecraft:sandstone_slab", 285}, {"minecraft:sandstone_stairs", 438}, {"minecraft:sandstone_wall", 468},
				{"minecraft:scaffolding", 716}, {"minecraft:scrape_pottery_sherd", 1446}, {"minecraft:sculk", 429}, {"minecraft:sculk_catalyst", 431},
				{"minecraft:sculk_sensor", 742}, {"minecraft:sculk_shrieker", 432}, {"minecraft:sculk_vein", 430}, {"minecraft:sea_lantern", 568},
				{"minecraft:sea_pickle", 212}, {"minecraft:seagrass", 211}, {"minecraft:sentry_armor_trim_smithing_template", 1410}, {"minecraft:shaper_armor_trim_smithing_template", 1422},
				{"minecraft:sheaf_pottery_sherd", 1447}, {"minecraft:shears", 1105}, {"minecraft:sheep_spawn_egg", 1182}, {"minecraft:shelter_pottery_sherd", 1448},
				{"minecraft:shield", 1290}, {"minecraft:short_dry_grass", 209}, {"minecraft:short_grass", 202}, {"minecraft:shroomlight", 1360},
				{"minecraft:shulker_box", 581}, {"minecraft:shulker_shell", 1292}, {"minecraft:shulker_spawn_egg", 1183}, {"minecraft:silence_armor_trim_smithing_template", 1423},
				{"minecraft:silverfish_spawn_egg", 1184}, {"minecraft:skeleton_horse_spawn_egg", 1186}, {"minecraft:skeleton_skull", 1229}, {"minecraft:skeleton_spawn_egg", 1185},
				{"minecraft:skull_banner_pattern", 1327}, {"minecraft:skull_pottery_sherd", 1449}, {"minecraft:slime_ball", 1030}, {"minecraft:slime_block", 724},
				{"minecraft:slime_spawn_egg", 1187}, {"minecraft:small_amethyst_bud", 1398}, {"minecraft:small_dripleaf", 268}, {"minecraft:smithing_table", 1343},
				{"minecraft:smoker", 1338}, {"minecraft:smooth_basalt", 364}, {"minecraft:smooth_quartz", 300}, {"minecraft:smooth_quartz_slab", 706},
				{"minecraft:smooth_quartz_stairs", 689}, {"minecraft:smooth_red_sandstone", 301}, {"minecraft:smooth_red_sandstone_slab", 700}, {"minecraft:smooth_red_sandstone_stairs", 682},
				{"minecraft:smooth_sandstone", 302}, {"minecraft:smooth_sandstone_slab", 705}, {"minecraft:smooth_sandstone_stairs", 688}, {"minecraft:smooth_stone", 303},
				{"minecraft:smooth_stone_slab", 284}, {"minecraft:sniffer_egg", 647}, {"minecraft:sniffer_spawn_egg", 1188}, {"minecraft:snort_pottery_sherd", 1450},
				{"minecraft:snout_armor_trim_smithing_template", 1418}, {"minecraft:snow", 337}, {"minecraft:snow_block", 339}, {"minecraft:snow_golem_spawn_egg", 1189},
				{"minecraft:snowball", 1016}, {"minecraft:soul_campfire", 1359}, {"minecraft:soul_lantern", 1347}, {"minecraft:soul_sand", 360},
				{"minecraft:soul_soil", 361}, {"minecraft:soul_torch", 365}, {"minecraft:spawner", 329}, {"minecraft:spectral_arrow", 1287},
				{"minecraft:spider_eye", 1122}, {"minecraft:spider_spawn_egg", 1190}, {"minecraft:spire_armor_trim_smithing_template", 1420}, {"minecraft:splash_potion", 1286},
				{"minecraft:sponge", 193}, {"minecraft:spore_blossom", 246}, {"minecraft:spruce_boat", 865}, {"minecraft:spruce_button", 752},
				{"minecraft:spruce_chest_boat", 866}, {"minecraft:spruce_door", 781}, {"minecraft:spruce_fence", 345}, {"minecraft:spruce_fence_gate", 822},
				{"minecraft:spruce_hanging_sign", 1001}, {"minecraft:spruce_leaves", 183}, {"minecraft:spruce_log", 135}, {"minecraft:spruce_planks", 37},
				{"minecraft:spruce_pressure_plate", 768}, {"minecraft:spruce_sapling", 50}, {"minecraft:spruce_shelf", 315}, {"minecraft:spruce_sign", 989},
				{"minecraft:spruce_slab", 271}, {"minecraft:spruce_stairs", 442}, {"minecraft:spruce_trapdoor", 802}, {"minecraft:spruce_wood", 172},
				{"minecraft:spyglass", 1055}, {"minecraft:squid_spawn_egg", 1191}, {"minecraft:stick", 946}, {"minecraft:sticky_piston", 723},
				{"minecraft:stone", 1}, {"minecraft:stone_axe", 924}, {"minecraft:stone_brick_slab", 290}, {"minecraft:stone_brick_stairs", 420},
				{"minecraft:stone_brick_wall", 463}, {"minecraft:stone_bricks", 375}, {"minecraft:stone_button", 749}, {"minecraft:stone_hoe", 925},
				{"minecraft:stone_pickaxe", 923}, {"minecraft:stone_pressure_plate", 763}, {"minecraft:stone_shovel", 922}, {"minecraft:stone_slab", 283},
				{"minecraft:stone_stairs", 687}, {"minecraft:stone_sword", 921}, {"minecraft:stonecutter", 1344}, {"minecraft:stray_spawn_egg", 1192},
				{"minecraft:strider_spawn_egg", 1193}, {"minecraft:string", 948}, {"minecraft:stripped_acacia_log", 152}, {"minecraft:stripped_acacia_wood", 163},
				{"minecraft:stripped_bamboo_block", 170}, {"minecraft:stripped_birch_log", 150}, {"minecraft:stripped_birch_wood", 161}, {"minecraft:stripped_cherry_log", 153},
				{"minecraft:stripped_cherry_wood", 164}, {"minecraft:stripped_crimson_hyphae", 168}, {"minecraft:stripped_crimson_stem", 157}, {"minecraft:stripped_dark_oak_log", 154},
				{"minecraft:stripped_dark_oak_wood", 165}, {"minecraft:stripped_jungle_log", 151}, {"minecraft:stripped_jungle_wood", 162}, {"minecraft:stripped_mangrove_log", 156},
				{"minecraft:stripped_mangrove_wood", 167}, {"minecraft:stripped_oak_log", 148}, {"minecraft:stripped_oak_wood", 159}, {"minecraft:stripped_pale_oak_log", 155},
				{"minecraft:stripped_pale_oak_wood", 166}, {"minecraft:stripped_spruce_log", 149}, {"minecraft:stripped_spruce_wood", 160}, {"minecraft:stripped_warped_hyphae", 169},
				{"minecraft:stripped_warped_stem", 158}, {"minecraft:structure_block", 883}, {"minecraft:structure_void", 580}, {"minecraft:sugar", 1084},
				{"minecraft:sugar_cane", 256}, {"minecraft:sunflower", 524}, {"minecraft:suspicious_gravel", 61}, {"minecraft:suspicious_sand", 60},
				{"minecraft:suspicious_stew", 1323}, {"minecraft:sweet_berries", 1356}, {"minecraft:tadpole_bucket", 1024}, {"minecraft:tadpole_spawn_egg", 1194},
				{"minecraft:tall_dry_grass", 210}, {"minecraft:tall_grass", 528}, {"minecraft:target", 731}, {"minecraft:terracotta", 521},
				{"minecraft:test_block", 885}, {"minecraft:test_instance_block", 886}, {"minecraft:tide_armor_trim_smithing_template", 1417}, {"minecraft:tinted_glass", 196},
				{"minecraft:tipped_arrow", 1288}, {"minecraft:tnt", 746}, {"minecraft:tnt_minecart", 857}, {"minecraft:torch", 322},
				{"minecraft:torchflower", 244}, {"minecraft:torchflower_seeds", 1280}, {"minecraft:totem_of_undying", 1291}, {"minecraft:trader_llama_spawn_egg", 1195},
				{"minecraft:trapped_chest", 745}, {"minecraft:trial_key", 1484}, {"minecraft:trial_spawner", 1483}, {"minecraft:trident", 1319},
				{"minecraft:tripwire_hook", 744}, {"minecraft:tropical_fish", 1059}, {"minecraft:tropical_fish_bucket", 1022}, {"minecraft:tropical_fish_spawn_egg", 1196},
				{"minecraft:tube_coral", 659}, {"minecraft:tube_coral_block", 654}, {"minecraft:tube_coral_fan", 669}, {"minecraft:tuff", 12},
				{"minecraft:tuff_brick_slab", 22}, {"minecraft:tuff_brick_stairs", 23}, {"minecraft:tuff_brick_wall", 24}, {"minecraft:tuff_bricks", 21},
				{"minecraft:tuff_slab", 13}, {"minecraft:tuff_stairs", 14}, {"minecraft:tuff_wall", 15}, {"minecraft:turtle_egg", 646},
				{"minecraft:turtle_helmet", 887}, {"minecraft:turtle_scute", 888}, {"minecraft:turtle_spawn_egg", 1197}, {"minecraft:twisting_vines", 255},
				{"minecraft:vault", 1486}, {"minecraft:verdant_froglight", 1404}, {"minecraft:vex_armor_trim_smithing_template", 1416}, {"minecraft:vex_spawn_egg", 1198},
				{"minecraft:villager_spawn_egg", 1199}, {"minecraft:vindicator_spawn_egg", 1200}, {"minecraft:vine", 410}, {"minecraft:wandering_trader_spawn_egg", 1201},
				{"minecraft:ward_armor_trim_smithing_template", 1414}, {"minecraft:warden_spawn_egg", 1202}, {"minecraft:warped_button", 762}, {"minecraft:warped_door", 791},
				{"minecraft:warped_fence", 355}, {"minecraft:warped_fence_gate", 832}, {"minecraft:warped_fungus", 250}, {"minecraft:warped_fungus_on_a_stick", 860},
				{"minecraft:warped_hanging_sign", 1011}, {"minecraft:warped_hyphae", 181}, {"minecraft:warped_nylium", 34}, {"minecraft:warped_planks", 47},
				{"minecraft:warped_pressure_plate", 778}, {"minecraft:warped_roots", 252}, {"minecraft:warped_shelf", 316}, {"minecraft:warped_sign", 999},
				{"minecraft:warped_slab", 282}, {"minecraft:warped_stairs", 453}, {"minecraft:warped_stem", 146}, {"minecraft:warped_trapdoor", 812},
				{"minecraft:warped_wart_block", 577}, {"minecraft:water_bucket", 1013}, {"minecraft:waxed_chiseled_copper", 118}, {"minecraft:waxed_copper_bars", 395},
				{"minecraft:waxed_copper_block", 114}, {"minecraft:waxed_copper_bulb", 1463}, {"minecraft:waxed_copper_chain", 404}, {"minecraft:waxed_copper_chest", 1471},
				{"minecraft:waxed_copper_door", 796}, {"minecraft:waxed_copper_golem_statue", 1479}, {"minecraft:waxed_copper_grate", 1455}, {"minecraft:waxed_copper_lantern", 1352},
				{"minecraft:waxed_copper_trapdoor", 817}, {"minecraft:waxed_cut_copper", 122}, {"minecraft:waxed_cut_copper_slab", 130}, {"minecraft:waxed_cut_copper_stairs", 126},
				{"minecraft:waxed_exposed_chiseled_copper", 119}, {"minecraft:waxed_exposed_copper", 115}, {"minecraft:waxed_exposed_copper_bars", 396}, {"minecraft:waxed_exposed_copper_bulb", 1464},
				{"minecraft:waxed_exposed_copper_chain", 405}, {"minecraft:waxed_exposed_copper_chest", 1472}, {"minecraft:waxed_exposed_copper_door", 797}, {"minecraft:waxed_exposed_copper_golem_statue", 1480},
				{"minecraft:waxed_exposed_copper_grate", 1456}, {"minecraft:waxed_exposed_copper_lantern", 1353}, {"minecraft:waxed_exposed_copper_trapdoor", 818}, {"minecraft:waxed_exposed_cut_copper", 123},
				{"minecraft:waxed_exposed_cut_copper_slab", 131}, {"minecraft:waxed_exposed_cut_copper_stairs", 127}, {"minecraft:waxed_exposed_lightning_rod", 738}, {"minecraft:waxed_lightning_rod", 737},
				{"minecraft:waxed_oxidized_chiseled_copper", 121}, {"minecraft:waxed_oxidized_copper", 117}, {"minecraft:waxed_oxidized_copper_bars", 398}, {"minecraft:waxed_oxidized_copper_bulb", 1466},
				{"minecraft:waxed_oxidized_copper_chain", 407}, {"minecraft:waxed_oxidized_copper_chest", 1474}, {"minecraft:waxed_oxidized_copper_door", 799}, {"minecraft:waxed_oxidized_copper_golem_statue", 1482},
				{"minecraft:waxed_oxidized_copper_grate", 1458}, {"minecraft:waxed_oxidized_copper_lantern", 1355}, {"minecraft:waxed_oxidized_copper_trapdoor", 820}, {"minecraft:waxed_oxidized_cut_copper", 125},
				{"minecraft:waxed_oxidized_cut_copper_slab", 133}, {"minecraft:waxed_oxidized_cut_copper_stairs", 129}, {"minecraft:waxed_oxidized_lightning_rod", 740}, {"minecraft:waxed_weathered_chiseled_copper", 120},
				{"minecraft:waxed_weathered_copper", 116}, {"minecraft:waxed_weathered_copper_bars", 397}, {"minecraft:waxed_weathered_copper_bulb", 1465}, {"minecraft:waxed_weathered_copper_chain", 406},
				{"minecraft:waxed_weathered_copper_chest", 1473}, {"minecraft:waxed_weathered_copper_door", 798}, {"minecraft:waxed_weathered_copper_golem_statue", 1481}, {"minecraft:waxed_weathered_copper_grate", 1457},
				{"minecraft:waxed_weathered_copper_lantern", 1354}, {"minecraft:waxed_weathered_copper_trapdoor", 819}, {"minecraft:waxed_weathered_cut_copper", 124}, {"minecraft:waxed_weathered_cut_copper_slab", 132},
				{"minecraft:waxed_weathered_cut_copper_stairs", 128}, {"minecraft:waxed_weathered_lightning_rod", 739}, {"minecraft:wayfinder_armor_trim_smithing_template", 1421}, {"minecraft:weathered_chiseled_copper", 100},
				{"minecraft:weathered_copper", 96}, {"minecraft:weathered_copper_bars", 393}, {"minecraft:weathered_copper_bulb", 1461}, {"minecraft:weathered_copper_chain", 402},
				{"minecraft:weathered_copper_chest", 1469}, {"minecraft:weathered_copper_door", 794}, {"minecraft:weathered_copper_golem_statue", 1477}, {"minecraft:weathered_copper_grate", 1453},
				{"minecraft:weathered_copper_lantern", 1350}, {"minecraft:weathered_copper_trapdoor", 815}, {"minecraft:weathered_cut_copper", 104}, {"minecraft:weathered_cut_copper_slab", 112},
				{"minecraft:weathered_cut_copper_stairs", 108}, {"minecraft:weathered_lightning_rod", 735}, {"minecraft:weeping_vines", 254}, {"minecraft:wet_sponge", 194},
				{"minecraft:wheat", 952}, {"minecraft:wheat_seeds", 951}, {"minecraft:white_banner", 1261}, {"minecraft:white_bed", 1086},
				{"minecraft:white_bundle", 1037}, {"minecraft:white_candle", 1382}, {"minecraft:white_carpet", 505}, {"minecraft:white_concrete", 614},
				{"minecraft:white_concrete_powder", 630}, {"minecraft:white_dye", 1066}, {"minecraft:white_glazed_terracotta", 598}, {"minecraft:white_harness", 838},
				{"minecraft:white_shulker_box", 582}, {"minecraft:white_stained_glass", 530}, {"minecraft:white_stained_glass_pane", 546}, {"minecraft:white_terracotta", 486},
				{"minecraft:white_tulip", 238}, {"minecraft:white_wool", 213}, {"minecraft:wild_armor_trim_smithing_template", 1413}, {"minecraft:wildflowers", 259},
				{"minecraft:wind_charge", 1215}, {"minecraft:witch_spawn_egg", 1203}, {"minecraft:wither_rose", 243}, {"minecraft:wither_skeleton_skull", 1230},
				{"minecraft:wither_skeleton_spawn_egg", 1205}, {"minecraft:wither_spawn_egg", 1204}, {"minecraft:wolf_armor", 890}, {"minecraft:wolf_spawn_egg", 1206},
				{"minecraft:wooden_axe", 914}, {"minecraft:wooden_hoe", 915}, {"minecraft:wooden_pickaxe", 913}, {"minecraft:wooden_shovel", 912},
				{"minecraft:wooden_sword", 911}, {"minecraft:writable_book", 1216}, {"minecraft:written_book", 1217}, {"minecraft:yellow_banner", 1265},
				{"minecraft:yellow_bed", 1090}, {"minecraft:yellow_bundle", 1041}, {"minecraft:yellow_candle", 1386}, {"minecraft:yellow_carpet", 509},
				{"minecraft:yellow_concrete", 618}, {"minecraft:yellow_concrete_powder", 634}, {"minecraft:yellow_dye", 1070}, {"minecraft:yellow_glazed_terracotta", 602},
				{"minecraft:yellow_harness", 842}, {"minecraft:yellow_shulker_box", 586}, {"minecraft:yellow_stained_glass", 534}, {"minecraft:yellow_stained_glass_pane", 550},
				{"minecraft:yellow_terracotta", 490}, {"minecraft:yellow_wool", 217}, {"minecraft:zoglin_spawn_egg", 1207}, {"minecraft:zombie_head", 1232},
				{"minecraft:zombie_horse_spawn_egg", 1210}, {"minecraft:zombie_spawn_egg", 1209}, {"minecraft:zombie_villager_spawn_egg", 1211}, {"minecraft:zombified_piglin_spawn_egg", 1212}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getLootConditionType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:all_of", 2}, {"minecraft:any_of", 1}, {"minecraft:block_state_property", 8}, {"minecraft:damage_source_properties", 12},
				{"minecraft:enchantment_active_check", 18}, {"minecraft:entity_properties", 5}, {"minecraft:entity_scores", 7}, {"minecraft:inverted", 0},
				{"minecraft:killed_by_player", 6}, {"minecraft:location_check", 13}, {"minecraft:match_tool", 9}, {"minecraft:random_chance", 3},
				{"minecraft:random_chance_with_enchanted_bonus", 4}, {"minecraft:reference", 15}, {"minecraft:survives_explosion", 11}, {"minecraft:table_bonus", 10},
				{"minecraft:time_check", 16}, {"minecraft:value_check", 17}, {"minecraft:weather_check", 14}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getLootFunctionType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:apply_bonus", 19}, {"minecraft:copy_components", 31}, {"minecraft:copy_custom_data", 24}, {"minecraft:copy_name", 14},
				{"minecraft:copy_state", 25}, {"minecraft:enchant_randomly", 3}, {"minecraft:enchant_with_levels", 2}, {"minecraft:enchanted_count_increase", 8},
				{"minecraft:exploration_map", 12}, {"minecraft:explosion_decay", 21}, {"minecraft:fill_player_head", 23}, {"minecraft:filtered", 17},
				{"minecraft:furnace_smelt", 7}, {"minecraft:limit_count", 18}, {"minecraft:modify_contents", 16}, {"minecraft:reference", 29},
				{"minecraft:sequence", 30}, {"minecraft:set_attributes", 10}, {"minecraft:set_banner_pattern", 26}, {"minecraft:set_book_cover", 34},
				{"minecraft:set_components", 6}, {"minecraft:set_contents", 15}, {"minecraft:set_count", 0}, {"minecraft:set_custom_data", 5},
				{"minecraft:set_custom_model_data", 39}, {"minecraft:set_damage", 9}, {"minecraft:set_enchantments", 4}, {"minecraft:set_firework_explosion", 33},
				{"minecraft:set_fireworks", 32}, {"minecraft:set_instrument", 28}, {"minecraft:set_item", 1}, {"minecraft:set_loot_table", 20},
				{"minecraft:set_lore", 22}, {"minecraft:set_name", 11}, {"minecraft:set_ominous_bottle_amplifier", 38}, {"minecraft:set_potion", 27},
				{"minecraft:set_stew_effect", 13}, {"minecraft:set_writable_book_pages", 36}, {"minecraft:set_written_book_pages", 35}, {"minecraft:toggle_tooltips", 37}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getLootNbtProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:context", 1}, {"minecraft:storage", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getLootNumberProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:binomial", 2}, {"minecraft:constant", 0}, {"minecraft:enchantment_level", 5}, {"minecraft:score", 3},
				{"minecraft:storage", 4}, {"minecraft:uniform", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getLootPoolEntryType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:alternatives", 5}, {"minecraft:dynamic", 3}, {"minecraft:empty", 0}, {"minecraft:group", 7},
				{"minecraft:item", 1}, {"minecraft:loot_table", 2}, {"minecraft:sequence", 6}, {"minecraft:tag", 4}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getLootScoreProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:context", 1}, {"minecraft:fixed", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getMapDecorationType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:banner_black", 25}, {"minecraft:banner_blue", 21}, {"minecraft:banner_brown", 22}, {"minecraft:banner_cyan", 19},
				{"minecraft:banner_gray", 17}, {"minecraft:banner_green", 23}, {"minecraft:banner_light_blue", 13}, {"minecraft:banner_light_gray", 18},
				{"minecraft:banner_lime", 15}, {"minecraft:banner_magenta", 12}, {"minecraft:banner_orange", 11}, {"minecraft:banner_pink", 16},
				{"minecraft:banner_purple", 20}, {"minecraft:banner_red", 24}, {"minecraft:banner_white", 10}, {"minecraft:banner_yellow", 14},
				{"minecraft:blue_marker", 3}, {"minecraft:frame", 1}, {"minecraft:jungle_temple", 32}, {"minecraft:mansion", 8},
				{"minecraft:monument", 9}, {"minecraft:player", 0}, {"minecraft:player_off_limits", 7}, {"minecraft:player_off_map", 6},
				{"minecraft:red_marker", 2}, {"minecraft:red_x", 26}, {"minecraft:swamp_hut", 33}, {"minecraft:target_point", 5},
				{"minecraft:target_x", 4}, {"minecraft:trial_chambers", 34}, {"minecraft:village_desert", 27}, {"minecraft:village_plains", 28},
				{"minecraft:village_savanna", 29}, {"minecraft:village_snowy", 30}, {"minecraft:village_taiga", 31}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getMemoryModuleType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:admiring_disabled", 62}, {"minecraft:admiring_item", 59}, {"minecraft:angry_at", 57}, {"minecraft:ate_recently", 77},
				{"minecraft:attack_cooling_down", 16}, {"minecraft:attack_target", 15}, {"minecraft:avoid_target", 26}, {"minecraft:breed_target", 18},
				{"minecraft:breeze_jump_cooldown", 102}, {"minecraft:breeze_jump_inhaling", 107}, {"minecraft:breeze_jump_target", 108}, {"minecraft:breeze_leaving_water", 109},
				{"minecraft:breeze_shoot", 103}, {"minecraft:breeze_shoot_charging", 104}, {"minecraft:breeze_shoot_cooldown", 106}, {"minecraft:breeze_shoot_recover", 105},
				{"minecraft:cant_reach_walk_target_since", 31}, {"minecraft:celebrate_location", 64}, {"minecraft:dancing", 65}, {"minecraft:danger_detected_recently", 33},
				{"minecraft:dig_cooldown", 86}, {"minecraft:disable_walk_to_admire_item", 61}, {"minecraft:disturbance_location", 81}, {"minecraft:doors_to_close", 22},
				{"minecraft:dummy", 0}, {"minecraft:gaze_cooldown_ticks", 43}, {"minecraft:golem_detected_recently", 32}, {"minecraft:has_hunting_cooldown", 47},
				{"minecraft:heard_bell_time", 30}, {"minecraft:hiding_place", 29}, {"minecraft:home", 1}, {"minecraft:hunted_recently", 63},
				{"minecraft:hurt_by", 24}, {"minecraft:hurt_by_entity", 25}, {"minecraft:interactable_doors", 21}, {"minecraft:interaction_target", 17},
				{"minecraft:is_emerging", 84}, {"minecraft:is_in_water", 50}, {"minecraft:is_panicking", 52}, {"minecraft:is_pregnant", 51},
				{"minecraft:is_sniffing", 83}, {"minecraft:is_tempted", 44}, {"minecraft:item_pickup_cooldown_ticks", 97}, {"minecraft:job_site", 2},
				{"minecraft:last_slept", 34}, {"minecraft:last_woken", 35}, {"minecraft:last_worked_at_poi", 36}, {"minecraft:liked_noteblock", 95},
				{"minecraft:liked_noteblock_cooldown_ticks", 96}, {"minecraft:liked_player", 94}, {"minecraft:long_jump_cooling_down", 45}, {"minecraft:long_jump_mid_jump", 46},
				{"minecraft:look_target", 14}, {"minecraft:meeting_point", 4}, {"minecraft:mobs", 6}, {"minecraft:nearby_adult_piglins", 69},
				{"minecraft:nearest_attackable", 28}, {"minecraft:nearest_bed", 23}, {"minecraft:nearest_hostile", 27}, {"minecraft:nearest_player_holding_wanted_item", 76},
				{"minecraft:nearest_players", 9}, {"minecraft:nearest_repellent", 78}, {"minecraft:nearest_targetable_player_not_wearing_gold", 68}, {"minecraft:nearest_visible_adult", 37},
				{"minecraft:nearest_visible_adult_hoglins", 71}, {"minecraft:nearest_visible_adult_piglin", 72}, {"minecraft:nearest_visible_adult_piglins", 70}, {"minecraft:nearest_visible_baby_hoglin", 67},
				{"minecraft:nearest_visible_huntable_hoglin", 66}, {"minecraft:nearest_visible_nemesis", 39}, {"minecraft:nearest_visible_player", 10}, {"minecraft:nearest_visible_targetable_player", 11},
				{"minecraft:nearest_visible_targetable_players", 12}, {"minecraft:nearest_visible_wanted_item", 38}, {"minecraft:nearest_visible_zombified", 73}, {"minecraft:pacified", 79},
				{"minecraft:path", 20}, {"minecraft:play_dead_ticks", 40}, {"minecraft:potential_job_site", 3}, {"minecraft:ram_cooldown_ticks", 48},
				{"minecraft:ram_target", 49}, {"minecraft:recent_projectile", 82}, {"minecraft:ride_target", 19}, {"minecraft:roar_sound_cooldown", 87},
				{"minecraft:roar_sound_delay", 85}, {"minecraft:roar_target", 80}, {"minecraft:secondary_job_site", 5}, {"minecraft:sniff_cooldown", 88},
				{"minecraft:sniffer_digging", 100}, {"minecraft:sniffer_explored_positions", 98}, {"minecraft:sniffer_happy", 101}, {"minecraft:sniffer_sniffing_target", 99},
				{"minecraft:sonic_boom_cooldown", 91}, {"minecraft:sonic_boom_sound_cooldown", 92}, {"minecraft:sonic_boom_sound_delay", 93}, {"minecraft:temptation_cooldown_ticks", 42},
				{"minecraft:tempting_player", 41}, {"minecraft:time_trying_to_reach_admire_item", 60}, {"minecraft:touch_cooldown", 89}, {"minecraft:transport_items_cooldown_ticks", 56},
				{"minecraft:universal_anger", 58}, {"minecraft:unreachable_tongue_targets", 53}, {"minecraft:unreachable_transport_block_positions", 55}, {"minecraft:vibration_cooldown", 90},
				{"minecraft:visible_adult_hoglin_count", 75}, {"minecraft:visible_adult_piglin_count", 74}, {"minecraft:visible_mobs", 7}, {"minecraft:visible_villager_babies", 8},
				{"minecraft:visited_block_positions", 54}, {"minecraft:walk_target", 13}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getMenu() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:anvil", 8}, {"minecraft:beacon", 9}, {"minecraft:blast_furnace", 10}, {"minecraft:brewing_stand", 11},
				{"minecraft:cartography_table", 23}, {"minecraft:crafter_3x3", 7}, {"minecraft:crafting", 12}, {"minecraft:enchantment", 13},
				{"minecraft:furnace", 14}, {"minecraft:generic_3x3", 6}, {"minecraft:generic_9x1", 0}, {"minecraft:generic_9x2", 1},
				{"minecraft:generic_9x3", 2}, {"minecraft:generic_9x4", 3}, {"minecraft:generic_9x5", 4}, {"minecraft:generic_9x6", 5},
				{"minecraft:grindstone", 15}, {"minecraft:hopper", 16}, {"minecraft:lectern", 17}, {"minecraft:loom", 18},
				{"minecraft:merchant", 19}, {"minecraft:shulker_box", 20}, {"minecraft:smithing", 21}, {"minecraft:smoker", 22},
				{"minecraft:stonecutter", 24}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getMobEffect() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:absorption", 21}, {"minecraft:bad_omen", 30}, {"minecraft:blindness", 14}, {"minecraft:conduit_power", 28},
				{"minecraft:darkness", 32}, {"minecraft:dolphins_grace", 29}, {"minecraft:fire_resistance", 11}, {"minecraft:glowing", 23},
				{"minecraft:haste", 2}, {"minecraft:health_boost", 20}, {"minecraft:hero_of_the_village", 31}, {"minecraft:hunger", 16},
				{"minecraft:infested", 38}, {"minecraft:instant_damage", 6}, {"minecraft:instant_health", 5}, {"minecraft:invisibility", 13},
				{"minecraft:jump_boost", 7}, {"minecraft:levitation", 24}, {"minecraft:luck", 25}, {"minecraft:mining_fatigue", 3},
				{"minecraft:nausea", 8}, {"minecraft:night_vision", 15}, {"minecraft:oozing", 37}, {"minecraft:poison", 18},
				{"minecraft:raid_omen", 34}, {"minecraft:regeneration", 9}, {"minecraft:resistance", 10}, {"minecraft:saturation", 22},
				{"minecraft:slow_falling", 27}, {"minecraft:slowness", 1}, {"minecraft:speed", 0}, {"minecraft:strength", 4},
				{"minecraft:trial_omen", 33}, {"minecraft:unluck", 26}, {"minecraft:water_breathing", 12}, {"minecraft:weakness", 17},
				{"minecraft:weaving", 36}, {"minecraft:wind_charged", 35}, {"minecraft:wither", 19}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getNumberFormatType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:blank", 0}, {"minecraft:fixed", 2}, {"minecraft:styled", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getOutgoingRpcMethods() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:notification/allowlist/added", 8}, {"minecraft:notification/allowlist/removed", 9}, {"minecraft:notification/bans/added", 12}, {"minecraft:notification/bans/removed", 13},
				{"minecraft:notification/gamerules/updated", 14}, {"minecraft:notification/ip_bans/added", 10}, {"minecraft:notification/ip_bans/removed", 11}, {"minecraft:notification/operators/added", 6},
				{"minecraft:notification/operators/removed", 7}, {"minecraft:notification/players/joined", 4}, {"minecraft:notification/players/left", 5}, {"minecraft:notification/server/saved", 3},
				{"minecraft:notification/server/saving", 2}, {"minecraft:notification/server/started", 0}, {"minecraft:notification/server/status", 15}, {"minecraft:notification/server/stopping", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getParticleType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:angry_villager", 0}, {"minecraft:ash", 82}, {"minecraft:block", 1}, {"minecraft:block_crumble", 113},
				{"minecraft:block_marker", 2}, {"minecraft:bubble", 3}, {"minecraft:bubble_column_up", 72}, {"minecraft:bubble_pop", 70},
				{"minecraft:campfire_cosy_smoke", 75}, {"minecraft:campfire_signal_smoke", 76}, {"minecraft:cherry_leaves", 34}, {"minecraft:cloud", 4},
				{"minecraft:composter", 44}, {"minecraft:copper_fire_flame", 5}, {"minecraft:crimson_spore", 83}, {"minecraft:crit", 6},
				{"minecraft:current_down", 71}, {"minecraft:damage_indicator", 7}, {"minecraft:dolphin", 74}, {"minecraft:dragon_breath", 8},
				{"minecraft:dripping_dripstone_lava", 93}, {"minecraft:dripping_dripstone_water", 95}, {"minecraft:dripping_honey", 77}, {"minecraft:dripping_lava", 9},
				{"minecraft:dripping_obsidian_tear", 86}, {"minecraft:dripping_water", 12}, {"minecraft:dust", 14}, {"minecraft:dust_color_transition", 15},
				{"minecraft:dust_pillar", 109}, {"minecraft:dust_plume", 105}, {"minecraft:effect", 16}, {"minecraft:egg_crack", 104},
				{"minecraft:elder_guardian", 17}, {"minecraft:electric_spark", 101}, {"minecraft:enchant", 19}, {"minecraft:enchanted_hit", 18},
				{"minecraft:end_rod", 20}, {"minecraft:entity_effect", 21}, {"minecraft:explosion", 23}, {"minecraft:explosion_emitter", 22},
				{"minecraft:falling_dripstone_lava", 94}, {"minecraft:falling_dripstone_water", 96}, {"minecraft:falling_dust", 29}, {"minecraft:falling_honey", 78},
				{"minecraft:falling_lava", 10}, {"minecraft:falling_nectar", 80}, {"minecraft:falling_obsidian_tear", 87}, {"minecraft:falling_spore_blossom", 81},
				{"minecraft:falling_water", 13}, {"minecraft:firefly", 114}, {"minecraft:firework", 30}, {"minecraft:fishing", 31},
				{"minecraft:flame", 32}, {"minecraft:flash", 42}, {"minecraft:glow", 98}, {"minecraft:glow_squid_ink", 97},
				{"minecraft:gust", 24}, {"minecraft:gust_emitter_large", 26}, {"minecraft:gust_emitter_small", 27}, {"minecraft:happy_villager", 43},
				{"minecraft:heart", 45}, {"minecraft:infested", 33}, {"minecraft:instant_effect", 46}, {"minecraft:item", 47},
				{"minecraft:item_cobweb", 51}, {"minecraft:item_slime", 50}, {"minecraft:item_snowball", 52}, {"minecraft:landing_honey", 79},
				{"minecraft:landing_lava", 11}, {"minecraft:landing_obsidian_tear", 88}, {"minecraft:large_smoke", 53}, {"minecraft:lava", 54},
				{"minecraft:mycelium", 55}, {"minecraft:nautilus", 73}, {"minecraft:note", 56}, {"minecraft:ominous_spawning", 110},
				{"minecraft:pale_oak_leaves", 35}, {"minecraft:poof", 57}, {"minecraft:portal", 58}, {"minecraft:raid_omen", 111},
				{"minecraft:rain", 59}, {"minecraft:reverse_portal", 89}, {"minecraft:scrape", 102}, {"minecraft:sculk_charge", 38},
				{"minecraft:sculk_charge_pop", 39}, {"minecraft:sculk_soul", 37}, {"minecraft:shriek", 103}, {"minecraft:small_flame", 91},
				{"minecraft:small_gust", 25}, {"minecraft:smoke", 60}, {"minecraft:sneeze", 62}, {"minecraft:snowflake", 92},
				{"minecraft:sonic_boom", 28}, {"minecraft:soul", 41}, {"minecraft:soul_fire_flame", 40}, {"minecraft:spit", 63},
				{"minecraft:splash", 68}, {"minecraft:spore_blossom_air", 85}, {"minecraft:squid_ink", 64}, {"minecraft:sweep_attack", 65},
				{"minecraft:tinted_leaves", 36}, {"minecraft:totem_of_undying", 66}, {"minecraft:trail", 49}, {"minecraft:trial_omen", 112},
				{"minecraft:trial_spawner_detection", 106}, {"minecraft:trial_spawner_detection_ominous", 107}, {"minecraft:underwater", 67}, {"minecraft:vault_connection", 108},
				{"minecraft:vibration", 48}, {"minecraft:warped_spore", 84}, {"minecraft:wax_off", 100}, {"minecraft:wax_on", 99},
				{"minecraft:white_ash", 90}, {"minecraft:white_smoke", 61}, {"minecraft:witch", 69}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getPointOfInterestType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:armorer", 0}, {"minecraft:bee_nest", 16}, {"minecraft:beehive", 15}, {"minecraft:butcher", 1},
				{"minecraft:cartographer", 2}, {"minecraft:cleric", 3}, {"minecraft:farmer", 4}, {"minecraft:fisherman", 5},
				{"minecraft:fletcher", 6}, {"minecraft:home", 13}, {"minecraft:leatherworker", 7}, {"minecraft:librarian", 8},
				{"minecraft:lightning_rod", 20}, {"minecraft:lodestone", 18}, {"minecraft:mason", 9}, {"minecraft:meeting", 14},
				{"minecraft:nether_portal", 17}, {"minecraft:shepherd", 10}, {"minecraft:test_instance", 19}, {"minecraft:toolsmith", 11},
				{"minecraft:weaponsmith", 12}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getPosRuleTest() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:always_true", 0}, {"minecraft:axis_aligned_linear_pos", 2}, {"minecraft:linear_pos", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getPositionSourceType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:block", 0}, {"minecraft:entity", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getPotion() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:awkward", 3}, {"minecraft:fire_resistance", 11}, {"minecraft:harming", 26}, {"minecraft:healing", 24},
				{"minecraft:infested", 45}, {"minecraft:invisibility", 6}, {"minecraft:leaping", 8}, {"minecraft:long_fire_resistance", 12},
				{"minecraft:long_invisibility", 7}, {"minecraft:long_leaping", 9}, {"minecraft:long_night_vision", 5}, {"minecraft:long_poison", 29},
				{"minecraft:long_regeneration", 32}, {"minecraft:long_slow_falling", 41}, {"minecraft:long_slowness", 17}, {"minecraft:long_strength", 35},
				{"minecraft:long_swiftness", 14}, {"minecraft:long_turtle_master", 20}, {"minecraft:long_water_breathing", 23}, {"minecraft:long_weakness", 38},
				{"minecraft:luck", 39}, {"minecraft:mundane", 1}, {"minecraft:night_vision", 4}, {"minecraft:oozing", 44},
				{"minecraft:poison", 28}, {"minecraft:regeneration", 31}, {"minecraft:slow_falling", 40}, {"minecraft:slowness", 16},
				{"minecraft:strength", 34}, {"minecraft:strong_harming", 27}, {"minecraft:strong_healing", 25}, {"minecraft:strong_leaping", 10},
				{"minecraft:strong_poison", 30}, {"minecraft:strong_regeneration", 33}, {"minecraft:strong_slowness", 18}, {"minecraft:strong_strength", 36},
				{"minecraft:strong_swiftness", 15}, {"minecraft:strong_turtle_master", 21}, {"minecraft:swiftness", 13}, {"minecraft:thick", 2},
				{"minecraft:turtle_master", 19}, {"minecraft:water", 0}, {"minecraft:water_breathing", 22}, {"minecraft:weakness", 37},
				{"minecraft:weaving", 43}, {"minecraft:wind_charged", 42}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getRecipeBookCategory() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:blast_furnace_blocks", 7}, {"minecraft:blast_furnace_misc", 8}, {"minecraft:campfire", 12}, {"minecraft:crafting_building_blocks", 0},
				{"minecraft:crafting_equipment", 2}, {"minecraft:crafting_misc", 3}, {"minecraft:crafting_redstone", 1}, {"minecraft:furnace_blocks", 5},
				{"minecraft:furnace_food", 4}, {"minecraft:furnace_misc", 6}, {"minecraft:smithing", 11}, {"minecraft:smoker_food", 9},
				{"minecraft:stonecutter", 10}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getRecipeDisplay() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:crafting_shaped", 1}, {"minecraft:crafting_shapeless", 0}, {"minecraft:furnace", 2}, {"minecraft:smithing", 4},
				{"minecraft:stonecutter", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getRecipeSerializer() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:blasting", 15}, {"minecraft:campfire_cooking", 17}, {"minecraft:crafting_decorated_pot", 21}, {"minecraft:crafting_shaped", 0},
				{"minecraft:crafting_shapeless", 1}, {"minecraft:crafting_special_armordye", 2}, {"minecraft:crafting_special_bannerduplicate", 10}, {"minecraft:crafting_special_bookcloning", 3},
				{"minecraft:crafting_special_firework_rocket", 6}, {"minecraft:crafting_special_firework_star", 7}, {"minecraft:crafting_special_firework_star_fade", 8}, {"minecraft:crafting_special_mapcloning", 4},
				{"minecraft:crafting_special_mapextending", 5}, {"minecraft:crafting_special_repairitem", 13}, {"minecraft:crafting_special_shielddecoration", 11}, {"minecraft:crafting_special_tippedarrow", 9},
				{"minecraft:crafting_transmute", 12}, {"minecraft:smelting", 14}, {"minecraft:smithing_transform", 19}, {"minecraft:smithing_trim", 20},
				{"minecraft:smoking", 16}, {"minecraft:stonecutting", 18}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getRecipeType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:blasting", 2}, {"minecraft:campfire_cooking", 4}, {"minecraft:crafting", 0}, {"minecraft:smelting", 1},
				{"minecraft:smithing", 6}, {"minecraft:smoking", 3}, {"minecraft:stonecutting", 5}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getRuleBlockEntityModifier() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:append_loot", 3}, {"minecraft:append_static", 2}, {"minecraft:clear", 0}, {"minecraft:passthrough", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getRuleTest() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:always_true", 0}, {"minecraft:block_match", 1}, {"minecraft:blockstate_match", 2}, {"minecraft:random_block_match", 4},
				{"minecraft:random_blockstate_match", 5}, {"minecraft:tag_match", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getSchedule() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:empty", 0}, {"minecraft:simple", 1}, {"minecraft:villager_baby", 2}, {"minecraft:villager_default", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getSensorType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:armadillo_scare_detected", 10}, {"minecraft:armadillo_temptations", 21}, {"minecraft:axolotl_attackables", 16}, {"minecraft:axolotl_temptations", 17},
				{"minecraft:breeze_attack_entity_sensor", 27}, {"minecraft:camel_temptations", 20}, {"minecraft:dummy", 0}, {"minecraft:frog_attackables", 23},
				{"minecraft:frog_temptations", 19}, {"minecraft:goat_temptations", 18}, {"minecraft:golem_detected", 9}, {"minecraft:happy_ghast_temptations", 22},
				{"minecraft:hoglin_specific_sensor", 13}, {"minecraft:hurt_by", 5}, {"minecraft:is_in_water", 24}, {"minecraft:nearest_adult", 14},
				{"minecraft:nearest_adult_any_type", 15}, {"minecraft:nearest_bed", 4}, {"minecraft:nearest_items", 1}, {"minecraft:nearest_living_entities", 2},
				{"minecraft:nearest_players", 3}, {"minecraft:piglin_brute_specific_sensor", 12}, {"minecraft:piglin_specific_sensor", 11}, {"minecraft:secondary_pois", 8},
				{"minecraft:sniffer_temptations", 26}, {"minecraft:villager_babies", 7}, {"minecraft:villager_hostiles", 6}, {"minecraft:warden_entity_sensor", 25}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getSlotDisplay() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:any_fuel", 1}, {"minecraft:composite", 7}, {"minecraft:empty", 0}, {"minecraft:item", 2},
				{"minecraft:item_stack", 3}, {"minecraft:smithing_trim", 5}, {"minecraft:tag", 4}, {"minecraft:with_remainder", 6}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getSoundEvent() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:ambient.basalt_deltas.additions", 8}, {"minecraft:ambient.basalt_deltas.loop", 9}, {"minecraft:ambient.basalt_deltas.mood", 10}, {"minecraft:ambient.cave", 7},
				{"minecraft:ambient.crimson_forest.additions", 11}, {"minecraft:ambient.crimson_forest.loop", 12}, {"minecraft:ambient.crimson_forest.mood", 13}, {"minecraft:ambient.nether_wastes.additions", 14},
				{"minecraft:ambient.nether_wastes.loop", 15}, {"minecraft:ambient.nether_wastes.mood", 16}, {"minecraft:ambient.soul_sand_valley.additions", 17}, {"minecraft:ambient.soul_sand_valley.loop", 18},
				{"minecraft:ambient.soul_sand_valley.mood", 19}, {"minecraft:ambient.underwater.enter", 23}, {"minecraft:ambient.underwater.exit", 24}, {"minecraft:ambient.underwater.loop", 25},
				{"minecraft:ambient.underwater.loop.additions", 26}, {"minecraft:ambient.underwater.loop.additions.rare", 27}, {"minecraft:ambient.underwater.loop.additions.ultra_rare", 28}, {"minecraft:ambient.warped_forest.additions", 20},
				{"minecraft:ambient.warped_forest.loop", 21}, {"minecraft:ambient.warped_forest.mood", 22}, {"minecraft:block.amethyst_block.break", 29}, {"minecraft:block.amethyst_block.chime", 30},
				{"minecraft:block.amethyst_block.fall", 31}, {"minecraft:block.amethyst_block.hit", 32}, {"minecraft:block.amethyst_block.place", 33}, {"minecraft:block.amethyst_block.resonate", 34},
				{"minecraft:block.amethyst_block.step", 35}, {"minecraft:block.amethyst_cluster.break", 36}, {"minecraft:block.amethyst_cluster.fall", 37}, {"minecraft:block.amethyst_cluster.hit", 38},
				{"minecraft:block.amethyst_cluster.place", 39}, {"minecraft:block.amethyst_cluster.step", 40}, {"minecraft:block.ancient_debris.break", 41}, {"minecraft:block.ancient_debris.fall", 45},
				{"minecraft:block.ancient_debris.hit", 44}, {"minecraft:block.ancient_debris.place", 43}, {"minecraft:block.ancient_debris.step", 42}, {"minecraft:block.anvil.break", 46},
				{"minecraft:block.anvil.destroy", 47}, {"minecraft:block.anvil.fall", 48}, {"minecraft:block.anvil.hit", 49}, {"minecraft:block.anvil.land", 50},
				{"minecraft:block.anvil.place", 51}, {"minecraft:block.anvil.step", 52}, {"minecraft:block.anvil.use", 53}, {"minecraft:block.azalea.break", 96},
				{"minecraft:block.azalea.fall", 97}, {"minecraft:block.azalea.hit", 98}, {"minecraft:block.azalea.place", 99}, {"minecraft:block.azalea.step", 100},
				{"minecraft:block.azalea_leaves.break", 101}, {"minecraft:block.azalea_leaves.fall", 102}, {"minecraft:block.azalea_leaves.hit", 103}, {"minecraft:block.azalea_leaves.place", 104},
				{"minecraft:block.azalea_leaves.step", 105}, {"minecraft:block.bamboo.break", 106}, {"minecraft:block.bamboo.fall", 107}, {"minecraft:block.bamboo.hit", 108},
				{"minecraft:block.bamboo.place", 109}, {"minecraft:block.bamboo.step", 110}, {"minecraft:block.bamboo_sapling.break", 111}, {"minecraft:block.bamboo_sapling.hit", 112},
				{"minecraft:block.bamboo_sapling.place", 113}, {"minecraft:block.bamboo_wood.break", 114}, {"minecraft:block.bamboo_wood.fall", 115}, {"minecraft:block.bamboo_wood.hit", 116},
				{"minecraft:block.bamboo_wood.place", 117}, {"minecraft:block.bamboo_wood.step", 118}, {"minecraft:block.bamboo_wood_button.click_off", 123}, {"minecraft:block.bamboo_wood_button.click_on", 124},
				{"minecraft:block.bamboo_wood_door.close", 119}, {"minecraft:block.bamboo_wood_door.open", 120}, {"minecraft:block.bamboo_wood_fence_gate.close", 127}, {"minecraft:block.bamboo_wood_fence_gate.open", 128},
				{"minecraft:block.bamboo_wood_hanging_sign.break", 745}, {"minecraft:block.bamboo_wood_hanging_sign.fall", 746}, {"minecraft:block.bamboo_wood_hanging_sign.hit", 747}, {"minecraft:block.bamboo_wood_hanging_sign.place", 748},
				{"minecraft:block.bamboo_wood_hanging_sign.step", 744}, {"minecraft:block.bamboo_wood_pressure_plate.click_off", 125}, {"minecraft:block.bamboo_wood_pressure_plate.click_on", 126}, {"minecraft:block.bamboo_wood_trapdoor.close", 121},
				{"minecraft:block.bamboo_wood_trapdoor.open", 122}, {"minecraft:block.barrel.close", 129}, {"minecraft:block.barrel.open", 130}, {"minecraft:block.basalt.break", 131},
				{"minecraft:block.basalt.fall", 135}, {"minecraft:block.basalt.hit", 134}, {"minecraft:block.basalt.place", 133}, {"minecraft:block.basalt.step", 132},
				{"minecraft:block.beacon.activate", 141}, {"minecraft:block.beacon.ambient", 142}, {"minecraft:block.beacon.deactivate", 143}, {"minecraft:block.beacon.power_select", 144},
				{"minecraft:block.beehive.drip", 151}, {"minecraft:block.beehive.enter", 152}, {"minecraft:block.beehive.exit", 153}, {"minecraft:block.beehive.shear", 154},
				{"minecraft:block.beehive.work", 155}, {"minecraft:block.bell.resonate", 157}, {"minecraft:block.bell.use", 156}, {"minecraft:block.big_dripleaf.break", 158},
				{"minecraft:block.big_dripleaf.fall", 159}, {"minecraft:block.big_dripleaf.hit", 160}, {"minecraft:block.big_dripleaf.place", 161}, {"minecraft:block.big_dripleaf.step", 162},
				{"minecraft:block.big_dripleaf.tilt_down", 507}, {"minecraft:block.big_dripleaf.tilt_up", 508}, {"minecraft:block.blastfurnace.fire_crackle", 183}, {"minecraft:block.bone_block.break", 175},
				{"minecraft:block.bone_block.fall", 176}, {"minecraft:block.bone_block.hit", 177}, {"minecraft:block.bone_block.place", 178}, {"minecraft:block.bone_block.step", 179},
				{"minecraft:block.brewing_stand.brew", 200}, {"minecraft:block.bubble_column.bubble_pop", 206}, {"minecraft:block.bubble_column.upwards_ambient", 207}, {"minecraft:block.bubble_column.upwards_inside", 208},
				{"minecraft:block.bubble_column.whirlpool_ambient", 209}, {"minecraft:block.bubble_column.whirlpool_inside", 210}, {"minecraft:block.cactus_flower.break", 228}, {"minecraft:block.cactus_flower.place", 229},
				{"minecraft:block.cake.add_candle", 230}, {"minecraft:block.calcite.break", 231}, {"minecraft:block.calcite.fall", 235}, {"minecraft:block.calcite.hit", 234},
				{"minecraft:block.calcite.place", 233}, {"minecraft:block.calcite.step", 232}, {"minecraft:block.campfire.crackle", 247}, {"minecraft:block.candle.ambient", 248},
				{"minecraft:block.candle.break", 249}, {"minecraft:block.candle.extinguish", 250}, {"minecraft:block.candle.fall", 251}, {"minecraft:block.candle.hit", 252},
				{"minecraft:block.candle.place", 253}, {"minecraft:block.candle.step", 254}, {"minecraft:block.cave_vines.break", 264}, {"minecraft:block.cave_vines.fall", 265},
				{"minecraft:block.cave_vines.hit", 266}, {"minecraft:block.cave_vines.pick_berries", 269}, {"minecraft:block.cave_vines.place", 267}, {"minecraft:block.cave_vines.step", 268},
				{"minecraft:block.chain.break", 270}, {"minecraft:block.chain.fall", 271}, {"minecraft:block.chain.hit", 272}, {"minecraft:block.chain.place", 273},
				{"minecraft:block.chain.step", 274}, {"minecraft:block.cherry_leaves.break", 285}, {"minecraft:block.cherry_leaves.fall", 286}, {"minecraft:block.cherry_leaves.hit", 287},
				{"minecraft:block.cherry_leaves.place", 288}, {"minecraft:block.cherry_leaves.step", 289}, {"minecraft:block.cherry_sapling.break", 280}, {"minecraft:block.cherry_sapling.fall", 281},
				{"minecraft:block.cherry_sapling.hit", 282}, {"minecraft:block.cherry_sapling.place", 283}, {"minecraft:block.cherry_sapling.step", 284}, {"minecraft:block.cherry_wood.break", 275},
				{"minecraft:block.cherry_wood.fall", 276}, {"minecraft:block.cherry_wood.hit", 277}, {"minecraft:block.cherry_wood.place", 278}, {"minecraft:block.cherry_wood.step", 279},
				{"minecraft:block.cherry_wood_button.click_off", 299}, {"minecraft:block.cherry_wood_button.click_on", 300}, {"minecraft:block.cherry_wood_door.close", 295}, {"minecraft:block.cherry_wood_door.open", 296},
				{"minecraft:block.cherry_wood_fence_gate.close", 303}, {"minecraft:block.cherry_wood_fence_gate.open", 304}, {"minecraft:block.cherry_wood_hanging_sign.break", 291}, {"minecraft:block.cherry_wood_hanging_sign.fall", 292},
				{"minecraft:block.cherry_wood_hanging_sign.hit", 293}, {"minecraft:block.cherry_wood_hanging_sign.place", 294}, {"minecraft:block.cherry_wood_hanging_sign.step", 290}, {"minecraft:block.cherry_wood_pressure_plate.click_off", 301},
				{"minecraft:block.cherry_wood_pressure_plate.click_on", 302}, {"minecraft:block.cherry_wood_trapdoor.close", 297}, {"minecraft:block.cherry_wood_trapdoor.open", 298}, {"minecraft:block.chest.close", 305},
				{"minecraft:block.chest.locked", 306}, {"minecraft:block.chest.open", 307}, {"minecraft:block.chiseled_bookshelf.break", 313}, {"minecraft:block.chiseled_bookshelf.fall", 314},
				{"minecraft:block.chiseled_bookshelf.hit", 315}, {"minecraft:block.chiseled_bookshelf.insert", 316}, {"minecraft:block.chiseled_bookshelf.insert.enchanted", 317}, {"minecraft:block.chiseled_bookshelf.pickup", 319},
				{"minecraft:block.chiseled_bookshelf.pickup.enchanted", 320}, {"minecraft:block.chiseled_bookshelf.place", 321}, {"minecraft:block.chiseled_bookshelf.step", 318}, {"minecraft:block.chorus_flower.death", 322},
				{"minecraft:block.chorus_flower.grow", 323}, {"minecraft:block.cobweb.break", 325}, {"minecraft:block.cobweb.fall", 329}, {"minecraft:block.cobweb.hit", 328},
				{"minecraft:block.cobweb.place", 327}, {"minecraft:block.cobweb.step", 326}, {"minecraft:block.comparator.click", 334}, {"minecraft:block.composter.empty", 335},
				{"minecraft:block.composter.fill", 336}, {"minecraft:block.composter.fill_success", 337}, {"minecraft:block.composter.ready", 338}, {"minecraft:block.conduit.activate", 339},
				{"minecraft:block.conduit.ambient", 340}, {"minecraft:block.conduit.ambient.short", 341}, {"minecraft:block.conduit.attack.target", 342}, {"minecraft:block.conduit.deactivate", 343},
				{"minecraft:block.copper.break", 351}, {"minecraft:block.copper.fall", 355}, {"minecraft:block.copper.hit", 354}, {"minecraft:block.copper.place", 353},
				{"minecraft:block.copper.step", 352}, {"minecraft:block.copper_bulb.break", 344}, {"minecraft:block.copper_bulb.fall", 348}, {"minecraft:block.copper_bulb.hit", 347},
				{"minecraft:block.copper_bulb.place", 346}, {"minecraft:block.copper_bulb.step", 345}, {"minecraft:block.copper_bulb.turn_off", 350}, {"minecraft:block.copper_bulb.turn_on", 349},
				{"minecraft:block.copper_chest.close", 356}, {"minecraft:block.copper_chest.open", 357}, {"minecraft:block.copper_chest_oxidized.close", 360}, {"minecraft:block.copper_chest_oxidized.open", 361},
				{"minecraft:block.copper_chest_weathered.close", 358}, {"minecraft:block.copper_chest_weathered.open", 359}, {"minecraft:block.copper_door.close", 362}, {"minecraft:block.copper_door.open", 363},
				{"minecraft:block.copper_golem_statue.break", 381}, {"minecraft:block.copper_golem_statue.fall", 385}, {"minecraft:block.copper_golem_statue.hit", 383}, {"minecraft:block.copper_golem_statue.place", 382},
				{"minecraft:block.copper_golem_statue.step", 384}, {"minecraft:block.copper_grate.break", 388}, {"minecraft:block.copper_grate.fall", 392}, {"minecraft:block.copper_grate.hit", 391},
				{"minecraft:block.copper_grate.place", 390}, {"minecraft:block.copper_grate.step", 389}, {"minecraft:block.copper_trapdoor.close", 393}, {"minecraft:block.copper_trapdoor.open", 394},
				{"minecraft:block.coral_block.break", 395}, {"minecraft:block.coral_block.fall", 396}, {"minecraft:block.coral_block.hit", 397}, {"minecraft:block.coral_block.place", 398},
				{"minecraft:block.coral_block.step", 399}, {"minecraft:block.crafter.craft", 405}, {"minecraft:block.crafter.fail", 406}, {"minecraft:block.creaking_heart.break", 418},
				{"minecraft:block.creaking_heart.fall", 419}, {"minecraft:block.creaking_heart.hit", 420}, {"minecraft:block.creaking_heart.hurt", 421}, {"minecraft:block.creaking_heart.idle", 424},
				{"minecraft:block.creaking_heart.place", 422}, {"minecraft:block.creaking_heart.spawn", 425}, {"minecraft:block.creaking_heart.step", 423}, {"minecraft:block.crop.break", 429},
				{"minecraft:block.deadbush.idle", 439}, {"minecraft:block.decorated_pot.break", 440}, {"minecraft:block.decorated_pot.fall", 441}, {"minecraft:block.decorated_pot.hit", 442},
				{"minecraft:block.decorated_pot.insert", 443}, {"minecraft:block.decorated_pot.insert_fail", 444}, {"minecraft:block.decorated_pot.place", 446}, {"minecraft:block.decorated_pot.shatter", 447},
				{"minecraft:block.decorated_pot.step", 445}, {"minecraft:block.deepslate.break", 453}, {"minecraft:block.deepslate.fall", 454}, {"minecraft:block.deepslate.hit", 455},
				{"minecraft:block.deepslate.place", 456}, {"minecraft:block.deepslate.step", 457}, {"minecraft:block.deepslate_bricks.break", 448}, {"minecraft:block.deepslate_bricks.fall", 449},
				{"minecraft:block.deepslate_bricks.hit", 450}, {"minecraft:block.deepslate_bricks.place", 451}, {"minecraft:block.deepslate_bricks.step", 452}, {"minecraft:block.deepslate_tiles.break", 458},
				{"minecraft:block.deepslate_tiles.fall", 459}, {"minecraft:block.deepslate_tiles.hit", 460}, {"minecraft:block.deepslate_tiles.place", 461}, {"minecraft:block.deepslate_tiles.step", 462},
				{"minecraft:block.dispenser.dispense", 463}, {"minecraft:block.dispenser.fail", 464}, {"minecraft:block.dispenser.launch", 465}, {"minecraft:block.dried_ghast.ambient", 486},
				{"minecraft:block.dried_ghast.ambient_water", 487}, {"minecraft:block.dried_ghast.break", 483}, {"minecraft:block.dried_ghast.fall", 485}, {"minecraft:block.dried_ghast.place", 488},
				{"minecraft:block.dried_ghast.place_in_water", 489}, {"minecraft:block.dried_ghast.step", 484}, {"minecraft:block.dried_ghast.transition", 490}, {"minecraft:block.dripstone_block.break", 491},
				{"minecraft:block.dripstone_block.fall", 495}, {"minecraft:block.dripstone_block.hit", 494}, {"minecraft:block.dripstone_block.place", 493}, {"minecraft:block.dripstone_block.step", 492},
				{"minecraft:block.dry_grass.ambient", 496}, {"minecraft:block.enchantment_table.use", 529}, {"minecraft:block.end_gateway.spawn", 552}, {"minecraft:block.end_portal.spawn", 554},
				{"minecraft:block.end_portal_frame.fill", 553}, {"minecraft:block.ender_chest.close", 530}, {"minecraft:block.ender_chest.open", 531}, {"minecraft:block.eyeblossom.close", 569},
				{"minecraft:block.eyeblossom.close_long", 568}, {"minecraft:block.eyeblossom.idle", 570}, {"minecraft:block.eyeblossom.open", 567}, {"minecraft:block.eyeblossom.open_long", 566},
				{"minecraft:block.fence_gate.close", 571}, {"minecraft:block.fence_gate.open", 572}, {"minecraft:block.fire.ambient", 583}, {"minecraft:block.fire.extinguish", 584},
				{"minecraft:block.firefly_bush.idle", 574}, {"minecraft:block.flowering_azalea.break", 590}, {"minecraft:block.flowering_azalea.fall", 591}, {"minecraft:block.flowering_azalea.hit", 592},
				{"minecraft:block.flowering_azalea.place", 593}, {"minecraft:block.flowering_azalea.step", 594}, {"minecraft:block.froglight.break", 616}, {"minecraft:block.froglight.fall", 617},
				{"minecraft:block.froglight.hit", 618}, {"minecraft:block.froglight.place", 619}, {"minecraft:block.froglight.step", 620}, {"minecraft:block.frogspawn.break", 622},
				{"minecraft:block.frogspawn.fall", 623}, {"minecraft:block.frogspawn.hatch", 624}, {"minecraft:block.frogspawn.hit", 625}, {"minecraft:block.frogspawn.place", 626},
				{"minecraft:block.frogspawn.step", 621}, {"minecraft:block.fungus.break", 1056}, {"minecraft:block.fungus.fall", 1060}, {"minecraft:block.fungus.hit", 1059},
				{"minecraft:block.fungus.place", 1058}, {"minecraft:block.fungus.step", 1057}, {"minecraft:block.furnace.fire_crackle", 640}, {"minecraft:block.gilded_blackstone.break", 662},
				{"minecraft:block.gilded_blackstone.fall", 663}, {"minecraft:block.gilded_blackstone.hit", 664}, {"minecraft:block.gilded_blackstone.place", 665}, {"minecraft:block.gilded_blackstone.step", 666},
				{"minecraft:block.glass.break", 667}, {"minecraft:block.glass.fall", 668}, {"minecraft:block.glass.hit", 669}, {"minecraft:block.glass.place", 670},
				{"minecraft:block.glass.step", 671}, {"minecraft:block.grass.break", 700}, {"minecraft:block.grass.fall", 701}, {"minecraft:block.grass.hit", 702},
				{"minecraft:block.grass.place", 703}, {"minecraft:block.grass.step", 704}, {"minecraft:block.gravel.break", 705}, {"minecraft:block.gravel.fall", 706},
				{"minecraft:block.gravel.hit", 707}, {"minecraft:block.gravel.place", 708}, {"minecraft:block.gravel.step", 709}, {"minecraft:block.grindstone.use", 710},
				{"minecraft:block.growing_plant.crop", 711}, {"minecraft:block.hanging_roots.break", 720}, {"minecraft:block.hanging_roots.fall", 721}, {"minecraft:block.hanging_roots.hit", 722},
				{"minecraft:block.hanging_roots.place", 723}, {"minecraft:block.hanging_roots.step", 724}, {"minecraft:block.hanging_sign.break", 726}, {"minecraft:block.hanging_sign.fall", 727},
				{"minecraft:block.hanging_sign.hit", 728}, {"minecraft:block.hanging_sign.place", 729}, {"minecraft:block.hanging_sign.step", 725}, {"minecraft:block.hanging_sign.waxed_interact_fail", 1637},
				{"minecraft:block.heavy_core.break", 734}, {"minecraft:block.heavy_core.fall", 735}, {"minecraft:block.heavy_core.hit", 736}, {"minecraft:block.heavy_core.place", 737},
				{"minecraft:block.heavy_core.step", 738}, {"minecraft:block.honey_block.break", 778}, {"minecraft:block.honey_block.fall", 779}, {"minecraft:block.honey_block.hit", 780},
				{"minecraft:block.honey_block.place", 781}, {"minecraft:block.honey_block.slide", 782}, {"minecraft:block.honey_block.step", 783}, {"minecraft:block.iron.break", 827},
				{"minecraft:block.iron.fall", 831}, {"minecraft:block.iron.hit", 830}, {"minecraft:block.iron.place", 829}, {"minecraft:block.iron.step", 828},
				{"minecraft:block.iron_door.close", 832}, {"minecraft:block.iron_door.open", 833}, {"minecraft:block.iron_trapdoor.close", 840}, {"minecraft:block.iron_trapdoor.open", 841},
				{"minecraft:block.ladder.break", 849}, {"minecraft:block.ladder.fall", 850}, {"minecraft:block.ladder.hit", 851}, {"minecraft:block.ladder.place", 852},
				{"minecraft:block.ladder.step", 853}, {"minecraft:block.lantern.break", 854}, {"minecraft:block.lantern.fall", 855}, {"minecraft:block.lantern.hit", 856},
				{"minecraft:block.lantern.place", 857}, {"minecraft:block.lantern.step", 858}, {"minecraft:block.large_amethyst_bud.break", 859}, {"minecraft:block.large_amethyst_bud.place", 860},
				{"minecraft:block.lava.ambient", 861}, {"minecraft:block.lava.extinguish", 862}, {"minecraft:block.lava.pop", 863}, {"minecraft:block.leaf_litter.break", 864},
				{"minecraft:block.leaf_litter.fall", 868}, {"minecraft:block.leaf_litter.hit", 867}, {"minecraft:block.leaf_litter.place", 866}, {"minecraft:block.leaf_litter.step", 865},
				{"minecraft:block.lever.click", 872}, {"minecraft:block.lily_pad.place", 1606}, {"minecraft:block.lodestone.break", 887}, {"minecraft:block.lodestone.fall", 891},
				{"minecraft:block.lodestone.hit", 890}, {"minecraft:block.lodestone.place", 889}, {"minecraft:block.lodestone.step", 888}, {"minecraft:block.mangrove_roots.break", 902},
				{"minecraft:block.mangrove_roots.fall", 903}, {"minecraft:block.mangrove_roots.hit", 904}, {"minecraft:block.mangrove_roots.place", 905}, {"minecraft:block.mangrove_roots.step", 906},
				{"minecraft:block.medium_amethyst_bud.break", 907}, {"minecraft:block.medium_amethyst_bud.place", 908}, {"minecraft:block.metal.break", 909}, {"minecraft:block.metal.fall", 910},
				{"minecraft:block.metal.hit", 911}, {"minecraft:block.metal.place", 912}, {"minecraft:block.metal.step", 915}, {"minecraft:block.metal_pressure_plate.click_off", 913},
				{"minecraft:block.metal_pressure_plate.click_on", 914}, {"minecraft:block.moss.break", 934}, {"minecraft:block.moss.fall", 935}, {"minecraft:block.moss.hit", 936},
				{"minecraft:block.moss.place", 937}, {"minecraft:block.moss.step", 938}, {"minecraft:block.moss_carpet.break", 924}, {"minecraft:block.moss_carpet.fall", 925},
				{"minecraft:block.moss_carpet.hit", 926}, {"minecraft:block.moss_carpet.place", 927}, {"minecraft:block.moss_carpet.step", 928}, {"minecraft:block.mud.break", 939},
				{"minecraft:block.mud.fall", 940}, {"minecraft:block.mud.hit", 941}, {"minecraft:block.mud.place", 942}, {"minecraft:block.mud.step", 943},
				{"minecraft:block.mud_bricks.break", 944}, {"minecraft:block.mud_bricks.fall", 945}, {"minecraft:block.mud_bricks.hit", 946}, {"minecraft:block.mud_bricks.place", 947},
				{"minecraft:block.mud_bricks.step", 948}, {"minecraft:block.muddy_mangrove_roots.break", 949}, {"minecraft:block.muddy_mangrove_roots.fall", 950}, {"minecraft:block.muddy_mangrove_roots.hit", 951},
				{"minecraft:block.muddy_mangrove_roots.place", 952}, {"minecraft:block.muddy_mangrove_roots.step", 953}, {"minecraft:block.nether_bricks.break", 1013}, {"minecraft:block.nether_bricks.fall", 1017},
				{"minecraft:block.nether_bricks.hit", 1016}, {"minecraft:block.nether_bricks.place", 1015}, {"minecraft:block.nether_bricks.step", 1014}, {"minecraft:block.nether_gold_ore.break", 1257},
				{"minecraft:block.nether_gold_ore.fall", 1258}, {"minecraft:block.nether_gold_ore.hit", 1259}, {"minecraft:block.nether_gold_ore.place", 1260}, {"minecraft:block.nether_gold_ore.step", 1261},
				{"minecraft:block.nether_ore.break", 1262}, {"minecraft:block.nether_ore.fall", 1263}, {"minecraft:block.nether_ore.hit", 1264}, {"minecraft:block.nether_ore.place", 1265},
				{"minecraft:block.nether_ore.step", 1266}, {"minecraft:block.nether_sprouts.break", 1051}, {"minecraft:block.nether_sprouts.fall", 1055}, {"minecraft:block.nether_sprouts.hit", 1054},
				{"minecraft:block.nether_sprouts.place", 1053}, {"minecraft:block.nether_sprouts.step", 1052}, {"minecraft:block.nether_wart.break", 1018}, {"minecraft:block.nether_wood.break", 1020},
				{"minecraft:block.nether_wood.fall", 1021}, {"minecraft:block.nether_wood.hit", 1022}, {"minecraft:block.nether_wood.place", 1023}, {"minecraft:block.nether_wood.step", 1024},
				{"minecraft:block.nether_wood_button.click_off", 1029}, {"minecraft:block.nether_wood_button.click_on", 1030}, {"minecraft:block.nether_wood_door.close", 1025}, {"minecraft:block.nether_wood_door.open", 1026},
				{"minecraft:block.nether_wood_fence_gate.close", 1033}, {"minecraft:block.nether_wood_fence_gate.open", 1034}, {"minecraft:block.nether_wood_hanging_sign.break", 740}, {"minecraft:block.nether_wood_hanging_sign.fall", 741},
				{"minecraft:block.nether_wood_hanging_sign.hit", 742}, {"minecraft:block.nether_wood_hanging_sign.place", 743}, {"minecraft:block.nether_wood_hanging_sign.step", 739}, {"minecraft:block.nether_wood_pressure_plate.click_off", 1031},
				{"minecraft:block.nether_wood_pressure_plate.click_on", 1032}, {"minecraft:block.nether_wood_trapdoor.close", 1027}, {"minecraft:block.nether_wood_trapdoor.open", 1028}, {"minecraft:block.netherite_block.break", 1071},
				{"minecraft:block.netherite_block.fall", 1075}, {"minecraft:block.netherite_block.hit", 1074}, {"minecraft:block.netherite_block.place", 1073}, {"minecraft:block.netherite_block.step", 1072},
				{"minecraft:block.netherrack.break", 1076}, {"minecraft:block.netherrack.fall", 1080}, {"minecraft:block.netherrack.hit", 1079}, {"minecraft:block.netherrack.place", 1078},
				{"minecraft:block.netherrack.step", 1077}, {"minecraft:block.note_block.banjo", 1096}, {"minecraft:block.note_block.basedrum", 1081}, {"minecraft:block.note_block.bass", 1082},
				{"minecraft:block.note_block.bell", 1083}, {"minecraft:block.note_block.bit", 1095}, {"minecraft:block.note_block.chime", 1084}, {"minecraft:block.note_block.cow_bell", 1093},
				{"minecraft:block.note_block.didgeridoo", 1094}, {"minecraft:block.note_block.flute", 1085}, {"minecraft:block.note_block.guitar", 1086}, {"minecraft:block.note_block.harp", 1087},
				{"minecraft:block.note_block.hat", 1088}, {"minecraft:block.note_block.imitate.creeper", 1099}, {"minecraft:block.note_block.imitate.ender_dragon", 1100}, {"minecraft:block.note_block.imitate.piglin", 1102},
				{"minecraft:block.note_block.imitate.skeleton", 1098}, {"minecraft:block.note_block.imitate.wither_skeleton", 1101}, {"minecraft:block.note_block.imitate.zombie", 1097}, {"minecraft:block.note_block.iron_xylophone", 1092},
				{"minecraft:block.note_block.pling", 1089}, {"minecraft:block.note_block.snare", 1090}, {"minecraft:block.note_block.xylophone", 1091}, {"minecraft:block.nylium.break", 1046},
				{"minecraft:block.nylium.fall", 1050}, {"minecraft:block.nylium.hit", 1049}, {"minecraft:block.nylium.place", 1048}, {"minecraft:block.nylium.step", 1047},
				{"minecraft:block.packed_mud.break", 1036}, {"minecraft:block.packed_mud.fall", 1037}, {"minecraft:block.packed_mud.hit", 1038}, {"minecraft:block.packed_mud.place", 1039},
				{"minecraft:block.packed_mud.step", 1040}, {"minecraft:block.pale_hanging_moss.idle", 1109}, {"minecraft:block.pink_petals.break", 929}, {"minecraft:block.pink_petals.fall", 930},
				{"minecraft:block.pink_petals.hit", 931}, {"minecraft:block.pink_petals.place", 932}, {"minecraft:block.pink_petals.step", 933}, {"minecraft:block.piston.contract", 1194},
				{"minecraft:block.piston.extend", 1195}, {"minecraft:block.pointed_dripstone.break", 497}, {"minecraft:block.pointed_dripstone.drip_lava", 503}, {"minecraft:block.pointed_dripstone.drip_lava_into_cauldron", 505},
				{"minecraft:block.pointed_dripstone.drip_water", 504}, {"minecraft:block.pointed_dripstone.drip_water_into_cauldron", 506}, {"minecraft:block.pointed_dripstone.fall", 501}, {"minecraft:block.pointed_dripstone.hit", 500},
				{"minecraft:block.pointed_dripstone.land", 502}, {"minecraft:block.pointed_dripstone.place", 499}, {"minecraft:block.pointed_dripstone.step", 498}, {"minecraft:block.polished_deepslate.break", 1223},
				{"minecraft:block.polished_deepslate.fall", 1224}, {"minecraft:block.polished_deepslate.hit", 1225}, {"minecraft:block.polished_deepslate.place", 1226}, {"minecraft:block.polished_deepslate.step", 1227},
				{"minecraft:block.polished_tuff.break", 1533}, {"minecraft:block.polished_tuff.fall", 1534}, {"minecraft:block.polished_tuff.hit", 1535}, {"minecraft:block.polished_tuff.place", 1536},
				{"minecraft:block.polished_tuff.step", 1537}, {"minecraft:block.portal.ambient", 1228}, {"minecraft:block.portal.travel", 1229}, {"minecraft:block.portal.trigger", 1230},
				{"minecraft:block.powder_snow.break", 1231}, {"minecraft:block.powder_snow.fall", 1232}, {"minecraft:block.powder_snow.hit", 1233}, {"minecraft:block.powder_snow.place", 1234},
				{"minecraft:block.powder_snow.step", 1235}, {"minecraft:block.pumpkin.carve", 1242}, {"minecraft:block.redstone_torch.burnout", 1267}, {"minecraft:block.resin.break", 1411},
				{"minecraft:block.resin.fall", 1412}, {"minecraft:block.resin.place", 1413}, {"minecraft:block.resin.step", 1414}, {"minecraft:block.resin_bricks.break", 1415},
				{"minecraft:block.resin_bricks.fall", 1416}, {"minecraft:block.resin_bricks.hit", 1417}, {"minecraft:block.resin_bricks.place", 1418}, {"minecraft:block.resin_bricks.step", 1419},
				{"minecraft:block.respawn_anchor.ambient", 1268}, {"minecraft:block.respawn_anchor.charge", 1269}, {"minecraft:block.respawn_anchor.deplete", 1270}, {"minecraft:block.respawn_anchor.set_spawn", 1271},
				{"minecraft:block.rooted_dirt.break", 1272}, {"minecraft:block.rooted_dirt.fall", 1273}, {"minecraft:block.rooted_dirt.hit", 1274}, {"minecraft:block.rooted_dirt.place", 1275},
				{"minecraft:block.rooted_dirt.step", 1276}, {"minecraft:block.roots.break", 635}, {"minecraft:block.roots.fall", 639}, {"minecraft:block.roots.hit", 638},
				{"minecraft:block.roots.place", 637}, {"minecraft:block.roots.step", 636}, {"minecraft:block.sand.break", 1281}, {"minecraft:block.sand.fall", 1282},
				{"minecraft:block.sand.hit", 1283}, {"minecraft:block.sand.idle", 1286}, {"minecraft:block.sand.place", 1284}, {"minecraft:block.sand.step", 1285},
				{"minecraft:block.scaffolding.break", 1287}, {"minecraft:block.scaffolding.fall", 1288}, {"minecraft:block.scaffolding.hit", 1289}, {"minecraft:block.scaffolding.place", 1290},
				{"minecraft:block.scaffolding.step", 1291}, {"minecraft:block.sculk.break", 1294}, {"minecraft:block.sculk.charge", 1293}, {"minecraft:block.sculk.fall", 1295},
				{"minecraft:block.sculk.hit", 1296}, {"minecraft:block.sculk.place", 1297}, {"minecraft:block.sculk.spread", 1292}, {"minecraft:block.sculk.step", 1298},
				{"minecraft:block.sculk_catalyst.bloom", 1299}, {"minecraft:block.sculk_catalyst.break", 1300}, {"minecraft:block.sculk_catalyst.fall", 1301}, {"minecraft:block.sculk_catalyst.hit", 1302},
				{"minecraft:block.sculk_catalyst.place", 1303}, {"minecraft:block.sculk_catalyst.step", 1304}, {"minecraft:block.sculk_sensor.break", 1307}, {"minecraft:block.sculk_sensor.clicking", 1305},
				{"minecraft:block.sculk_sensor.clicking_stop", 1306}, {"minecraft:block.sculk_sensor.fall", 1308}, {"minecraft:block.sculk_sensor.hit", 1309}, {"minecraft:block.sculk_sensor.place", 1310},
				{"minecraft:block.sculk_sensor.step", 1311}, {"minecraft:block.sculk_shrieker.break", 1312}, {"minecraft:block.sculk_shrieker.fall", 1313}, {"minecraft:block.sculk_shrieker.hit", 1314},
				{"minecraft:block.sculk_shrieker.place", 1315}, {"minecraft:block.sculk_shrieker.shriek", 1316}, {"minecraft:block.sculk_shrieker.step", 1317}, {"minecraft:block.sculk_vein.break", 1318},
				{"minecraft:block.sculk_vein.fall", 1319}, {"minecraft:block.sculk_vein.hit", 1320}, {"minecraft:block.sculk_vein.place", 1321}, {"minecraft:block.sculk_vein.step", 1322},
				{"minecraft:block.shelf.activate", 1329}, {"minecraft:block.shelf.break", 1330}, {"minecraft:block.shelf.deactivate", 1331}, {"minecraft:block.shelf.fall", 1332},
				{"minecraft:block.shelf.hit", 1333}, {"minecraft:block.shelf.multi_swap", 1334}, {"minecraft:block.shelf.place", 1335}, {"minecraft:block.shelf.place_item", 1336},
				{"minecraft:block.shelf.single_swap", 1337}, {"minecraft:block.shelf.step", 1338}, {"minecraft:block.shelf.take_item", 1339}, {"minecraft:block.shroomlight.break", 1342},
				{"minecraft:block.shroomlight.fall", 1346}, {"minecraft:block.shroomlight.hit", 1345}, {"minecraft:block.shroomlight.place", 1344}, {"minecraft:block.shroomlight.step", 1343},
				{"minecraft:block.shulker_box.close", 1349}, {"minecraft:block.shulker_box.open", 1350}, {"minecraft:block.sign.waxed_interact_fail", 1638}, {"minecraft:block.slime_block.break", 1383},
				{"minecraft:block.slime_block.fall", 1384}, {"minecraft:block.slime_block.hit", 1385}, {"minecraft:block.slime_block.place", 1386}, {"minecraft:block.slime_block.step", 1387},
				{"minecraft:block.small_amethyst_bud.break", 1388}, {"minecraft:block.small_amethyst_bud.place", 1389}, {"minecraft:block.small_dripleaf.break", 1390}, {"minecraft:block.small_dripleaf.fall", 1391},
				{"minecraft:block.small_dripleaf.hit", 1392}, {"minecraft:block.small_dripleaf.place", 1393}, {"minecraft:block.small_dripleaf.step", 1394}, {"minecraft:block.smithing_table.use", 1438},
				{"minecraft:block.smoker.smoke", 1439}, {"minecraft:block.sniffer_egg.crack", 1453}, {"minecraft:block.sniffer_egg.hatch", 1454}, {"minecraft:block.sniffer_egg.plop", 1452},
				{"minecraft:block.snow.break", 1456}, {"minecraft:block.snow.fall", 1457}, {"minecraft:block.snow.hit", 1463}, {"minecraft:block.snow.place", 1464},
				{"minecraft:block.snow.step", 1465}, {"minecraft:block.soul_sand.break", 1395}, {"minecraft:block.soul_sand.fall", 1399}, {"minecraft:block.soul_sand.hit", 1398},
				{"minecraft:block.soul_sand.place", 1397}, {"minecraft:block.soul_sand.step", 1396}, {"minecraft:block.soul_soil.break", 1400}, {"minecraft:block.soul_soil.fall", 1404},
				{"minecraft:block.soul_soil.hit", 1403}, {"minecraft:block.soul_soil.place", 1402}, {"minecraft:block.soul_soil.step", 1401}, {"minecraft:block.spawner.break", 1406},
				{"minecraft:block.spawner.fall", 1407}, {"minecraft:block.spawner.hit", 1408}, {"minecraft:block.spawner.place", 1409}, {"minecraft:block.spawner.step", 1410},
				{"minecraft:block.sponge.absorb", 1477}, {"minecraft:block.sponge.break", 1472}, {"minecraft:block.sponge.fall", 1473}, {"minecraft:block.sponge.hit", 1474},
				{"minecraft:block.sponge.place", 1475}, {"minecraft:block.sponge.step", 1476}, {"minecraft:block.spore_blossom.break", 1420}, {"minecraft:block.spore_blossom.fall", 1421},
				{"minecraft:block.spore_blossom.hit", 1422}, {"minecraft:block.spore_blossom.place", 1423}, {"minecraft:block.spore_blossom.step", 1424}, {"minecraft:block.stem.break", 1041},
				{"minecraft:block.stem.fall", 1045}, {"minecraft:block.stem.hit", 1044}, {"minecraft:block.stem.place", 1043}, {"minecraft:block.stem.step", 1042},
				{"minecraft:block.stone.break", 1484}, {"minecraft:block.stone.fall", 1487}, {"minecraft:block.stone.hit", 1488}, {"minecraft:block.stone.place", 1489},
				{"minecraft:block.stone.step", 1492}, {"minecraft:block.stone_button.click_off", 1485}, {"minecraft:block.stone_button.click_on", 1486}, {"minecraft:block.stone_pressure_plate.click_off", 1490},
				{"minecraft:block.stone_pressure_plate.click_on", 1491}, {"minecraft:block.suspicious_gravel.break", 611}, {"minecraft:block.suspicious_gravel.fall", 615}, {"minecraft:block.suspicious_gravel.hit", 614},
				{"minecraft:block.suspicious_gravel.place", 613}, {"minecraft:block.suspicious_gravel.step", 612}, {"minecraft:block.suspicious_sand.break", 606}, {"minecraft:block.suspicious_sand.fall", 610},
				{"minecraft:block.suspicious_sand.hit", 609}, {"minecraft:block.suspicious_sand.place", 608}, {"minecraft:block.suspicious_sand.step", 607}, {"minecraft:block.sweet_berry_bush.break", 1497},
				{"minecraft:block.sweet_berry_bush.pick_berries", 1499}, {"minecraft:block.sweet_berry_bush.place", 1498}, {"minecraft:block.trial_spawner.about_to_spawn_item", 755}, {"minecraft:block.trial_spawner.ambient", 760},
				{"minecraft:block.trial_spawner.ambient_ominous", 761}, {"minecraft:block.trial_spawner.break", 749}, {"minecraft:block.trial_spawner.close_shutter", 763}, {"minecraft:block.trial_spawner.detect_player", 758},
				{"minecraft:block.trial_spawner.eject_item", 764}, {"minecraft:block.trial_spawner.fall", 753}, {"minecraft:block.trial_spawner.hit", 752}, {"minecraft:block.trial_spawner.ominous_activate", 759},
				{"minecraft:block.trial_spawner.open_shutter", 762}, {"minecraft:block.trial_spawner.place", 751}, {"minecraft:block.trial_spawner.spawn_item", 756}, {"minecraft:block.trial_spawner.spawn_item_begin", 757},
				{"minecraft:block.trial_spawner.spawn_mob", 754}, {"minecraft:block.trial_spawner.step", 750}, {"minecraft:block.tripwire.attach", 1515}, {"minecraft:block.tripwire.click_off", 1516},
				{"minecraft:block.tripwire.click_on", 1517}, {"minecraft:block.tripwire.detach", 1518}, {"minecraft:block.tuff.break", 1523}, {"minecraft:block.tuff.fall", 1527},
				{"minecraft:block.tuff.hit", 1526}, {"minecraft:block.tuff.place", 1525}, {"minecraft:block.tuff.step", 1524}, {"minecraft:block.tuff_bricks.break", 1528},
				{"minecraft:block.tuff_bricks.fall", 1529}, {"minecraft:block.tuff_bricks.hit", 1530}, {"minecraft:block.tuff_bricks.place", 1531}, {"minecraft:block.tuff_bricks.step", 1532},
				{"minecraft:block.vault.activate", 1559}, {"minecraft:block.vault.ambient", 1560}, {"minecraft:block.vault.break", 1561}, {"minecraft:block.vault.close_shutter", 1562},
				{"minecraft:block.vault.deactivate", 1563}, {"minecraft:block.vault.eject_item", 1564}, {"minecraft:block.vault.fall", 1566}, {"minecraft:block.vault.hit", 1567},
				{"minecraft:block.vault.insert_item", 1568}, {"minecraft:block.vault.insert_item_fail", 1569}, {"minecraft:block.vault.open_shutter", 1570}, {"minecraft:block.vault.place", 1571},
				{"minecraft:block.vault.reject_rewarded_player", 1565}, {"minecraft:block.vault.step", 1572}, {"minecraft:block.vine.break", 1601}, {"minecraft:block.vine.fall", 1602},
				{"minecraft:block.vine.hit", 1603}, {"minecraft:block.vine.place", 1604}, {"minecraft:block.vine.step", 1605}, {"minecraft:block.wart_block.break", 1066},
				{"minecraft:block.wart_block.fall", 1070}, {"minecraft:block.wart_block.hit", 1069}, {"minecraft:block.wart_block.place", 1068}, {"minecraft:block.wart_block.step", 1067},
				{"minecraft:block.water.ambient", 1639}, {"minecraft:block.weeping_vines.break", 1061}, {"minecraft:block.weeping_vines.fall", 1065}, {"minecraft:block.weeping_vines.hit", 1064},
				{"minecraft:block.weeping_vines.place", 1063}, {"minecraft:block.weeping_vines.step", 1062}, {"minecraft:block.wet_grass.break", 1643}, {"minecraft:block.wet_grass.fall", 1644},
				{"minecraft:block.wet_grass.hit", 1645}, {"minecraft:block.wet_grass.place", 1646}, {"minecraft:block.wet_grass.step", 1647}, {"minecraft:block.wet_sponge.break", 1648},
				{"minecraft:block.wet_sponge.dries", 1649}, {"minecraft:block.wet_sponge.fall", 1650}, {"minecraft:block.wet_sponge.hit", 1651}, {"minecraft:block.wet_sponge.place", 1652},
				{"minecraft:block.wet_sponge.step", 1653}, {"minecraft:block.wood.break", 1728}, {"minecraft:block.wood.fall", 1729}, {"minecraft:block.wood.hit", 1730},
				{"minecraft:block.wood.place", 1731}, {"minecraft:block.wood.step", 1732}, {"minecraft:block.wooden_button.click_off", 1724}, {"minecraft:block.wooden_button.click_on", 1725},
				{"minecraft:block.wooden_door.close", 1720}, {"minecraft:block.wooden_door.open", 1721}, {"minecraft:block.wooden_pressure_plate.click_off", 1726}, {"minecraft:block.wooden_pressure_plate.click_on", 1727},
				{"minecraft:block.wooden_trapdoor.close", 1722}, {"minecraft:block.wooden_trapdoor.open", 1723}, {"minecraft:block.wool.break", 1733}, {"minecraft:block.wool.fall", 1734},
				{"minecraft:block.wool.hit", 1735}, {"minecraft:block.wool.place", 1736}, {"minecraft:block.wool.step", 1737}, {"minecraft:enchant.thorns.hit", 1504},
				{"minecraft:entity.allay.ambient_with_item", 0}, {"minecraft:entity.allay.ambient_without_item", 1}, {"minecraft:entity.allay.death", 2}, {"minecraft:entity.allay.hurt", 3},
				{"minecraft:entity.allay.item_given", 4}, {"minecraft:entity.allay.item_taken", 5}, {"minecraft:entity.allay.item_thrown", 6}, {"minecraft:entity.armadillo.ambient", 57},
				{"minecraft:entity.armadillo.brush", 66}, {"minecraft:entity.armadillo.death", 59}, {"minecraft:entity.armadillo.eat", 54}, {"minecraft:entity.armadillo.hurt", 55},
				{"minecraft:entity.armadillo.hurt_reduced", 56}, {"minecraft:entity.armadillo.land", 61}, {"minecraft:entity.armadillo.peek", 64}, {"minecraft:entity.armadillo.roll", 60},
				{"minecraft:entity.armadillo.scute_drop", 62}, {"minecraft:entity.armadillo.step", 58}, {"minecraft:entity.armadillo.unroll_finish", 63}, {"minecraft:entity.armadillo.unroll_start", 65},
				{"minecraft:entity.armor_stand.break", 79}, {"minecraft:entity.armor_stand.fall", 80}, {"minecraft:entity.armor_stand.hit", 81}, {"minecraft:entity.armor_stand.place", 82},
				{"minecraft:entity.arrow.hit", 83}, {"minecraft:entity.arrow.hit_player", 84}, {"minecraft:entity.arrow.shoot", 85}, {"minecraft:entity.axolotl.attack", 89},
				{"minecraft:entity.axolotl.death", 90}, {"minecraft:entity.axolotl.hurt", 91}, {"minecraft:entity.axolotl.idle_air", 92}, {"minecraft:entity.axolotl.idle_water", 93},
				{"minecraft:entity.axolotl.splash", 94}, {"minecraft:entity.axolotl.swim", 95}, {"minecraft:entity.bat.ambient", 136}, {"minecraft:entity.bat.death", 137},
				{"minecraft:entity.bat.hurt", 138}, {"minecraft:entity.bat.loop", 139}, {"minecraft:entity.bat.takeoff", 140}, {"minecraft:entity.bee.death", 145},
				{"minecraft:entity.bee.hurt", 146}, {"minecraft:entity.bee.loop", 148}, {"minecraft:entity.bee.loop_aggressive", 147}, {"minecraft:entity.bee.pollinate", 150},
				{"minecraft:entity.bee.sting", 149}, {"minecraft:entity.blaze.ambient", 163}, {"minecraft:entity.blaze.burn", 164}, {"minecraft:entity.blaze.death", 165},
				{"minecraft:entity.blaze.hurt", 166}, {"minecraft:entity.blaze.shoot", 167}, {"minecraft:entity.boat.paddle_land", 168}, {"minecraft:entity.boat.paddle_water", 169},
				{"minecraft:entity.bogged.ambient", 170}, {"minecraft:entity.bogged.death", 171}, {"minecraft:entity.bogged.hurt", 172}, {"minecraft:entity.bogged.shear", 173},
				{"minecraft:entity.bogged.step", 174}, {"minecraft:entity.breeze.charge", 187}, {"minecraft:entity.breeze.death", 196}, {"minecraft:entity.breeze.deflect", 188},
				{"minecraft:entity.breeze.hurt", 197}, {"minecraft:entity.breeze.idle_air", 191}, {"minecraft:entity.breeze.idle_ground", 190}, {"minecraft:entity.breeze.inhale", 189},
				{"minecraft:entity.breeze.jump", 193}, {"minecraft:entity.breeze.land", 194}, {"minecraft:entity.breeze.shoot", 192}, {"minecraft:entity.breeze.slide", 195},
				{"minecraft:entity.breeze.whirl", 198}, {"minecraft:entity.breeze.wind_burst", 199}, {"minecraft:entity.camel.ambient", 236}, {"minecraft:entity.camel.dash", 237},
				{"minecraft:entity.camel.dash_ready", 238}, {"minecraft:entity.camel.death", 239}, {"minecraft:entity.camel.eat", 240}, {"minecraft:entity.camel.hurt", 241},
				{"minecraft:entity.camel.saddle", 242}, {"minecraft:entity.camel.sit", 243}, {"minecraft:entity.camel.stand", 244}, {"minecraft:entity.camel.step", 245},
				{"minecraft:entity.camel.step_sand", 246}, {"minecraft:entity.cat.ambient", 255}, {"minecraft:entity.cat.beg_for_food", 260}, {"minecraft:entity.cat.death", 257},
				{"minecraft:entity.cat.eat", 258}, {"minecraft:entity.cat.hiss", 259}, {"minecraft:entity.cat.hurt", 261}, {"minecraft:entity.cat.purr", 262},
				{"minecraft:entity.cat.purreow", 263}, {"minecraft:entity.cat.stray_ambient", 256}, {"minecraft:entity.chicken.ambient", 308}, {"minecraft:entity.chicken.death", 309},
				{"minecraft:entity.chicken.egg", 310}, {"minecraft:entity.chicken.hurt", 311}, {"minecraft:entity.chicken.step", 312}, {"minecraft:entity.cod.ambient", 330},
				{"minecraft:entity.cod.death", 331}, {"minecraft:entity.cod.flop", 332}, {"minecraft:entity.cod.hurt", 333}, {"minecraft:entity.copper_golem.death", 366},
				{"minecraft:entity.copper_golem.hurt", 365}, {"minecraft:entity.copper_golem.item_drop", 378}, {"minecraft:entity.copper_golem.item_no_drop", 379}, {"minecraft:entity.copper_golem.no_item_get", 376},
				{"minecraft:entity.copper_golem.no_item_no_get", 377}, {"minecraft:entity.copper_golem.shear", 387}, {"minecraft:entity.copper_golem.spawn", 386}, {"minecraft:entity.copper_golem.spin", 373},
				{"minecraft:entity.copper_golem.step", 364}, {"minecraft:entity.copper_golem_become_statue", 380}, {"minecraft:entity.copper_golem_oxidized.death", 372}, {"minecraft:entity.copper_golem_oxidized.hurt", 371},
				{"minecraft:entity.copper_golem_oxidized.spin", 375}, {"minecraft:entity.copper_golem_oxidized.step", 370}, {"minecraft:entity.copper_golem_weathered.death", 369}, {"minecraft:entity.copper_golem_weathered.hurt", 368},
				{"minecraft:entity.copper_golem_weathered.spin", 374}, {"minecraft:entity.copper_golem_weathered.step", 367}, {"minecraft:entity.cow.ambient", 400}, {"minecraft:entity.cow.death", 401},
				{"minecraft:entity.cow.hurt", 402}, {"minecraft:entity.cow.milk", 403}, {"minecraft:entity.cow.step", 404}, {"minecraft:entity.creaking.activate", 408},
				{"minecraft:entity.creaking.ambient", 407}, {"minecraft:entity.creaking.attack", 410}, {"minecraft:entity.creaking.deactivate", 409}, {"minecraft:entity.creaking.death", 411},
				{"minecraft:entity.creaking.freeze", 413}, {"minecraft:entity.creaking.spawn", 415}, {"minecraft:entity.creaking.step", 412}, {"minecraft:entity.creaking.sway", 416},
				{"minecraft:entity.creaking.twitch", 417}, {"minecraft:entity.creaking.unfreeze", 414}, {"minecraft:entity.creeper.death", 426}, {"minecraft:entity.creeper.hurt", 427},
				{"minecraft:entity.creeper.primed", 428}, {"minecraft:entity.dolphin.ambient", 466}, {"minecraft:entity.dolphin.ambient_water", 467}, {"minecraft:entity.dolphin.attack", 468},
				{"minecraft:entity.dolphin.death", 469}, {"minecraft:entity.dolphin.eat", 470}, {"minecraft:entity.dolphin.hurt", 471}, {"minecraft:entity.dolphin.jump", 472},
				{"minecraft:entity.dolphin.play", 473}, {"minecraft:entity.dolphin.splash", 474}, {"minecraft:entity.dolphin.swim", 475}, {"minecraft:entity.donkey.ambient", 476},
				{"minecraft:entity.donkey.angry", 477}, {"minecraft:entity.donkey.chest", 478}, {"minecraft:entity.donkey.death", 479}, {"minecraft:entity.donkey.eat", 480},
				{"minecraft:entity.donkey.hurt", 481}, {"minecraft:entity.donkey.jump", 482}, {"minecraft:entity.dragon_fireball.explode", 534}, {"minecraft:entity.drowned.ambient", 509},
				{"minecraft:entity.drowned.ambient_water", 510}, {"minecraft:entity.drowned.death", 511}, {"minecraft:entity.drowned.death_water", 512}, {"minecraft:entity.drowned.hurt", 513},
				{"minecraft:entity.drowned.hurt_water", 514}, {"minecraft:entity.drowned.shoot", 515}, {"minecraft:entity.drowned.step", 516}, {"minecraft:entity.drowned.swim", 517},
				{"minecraft:entity.egg.throw", 519}, {"minecraft:entity.elder_guardian.ambient", 520}, {"minecraft:entity.elder_guardian.ambient_land", 521}, {"minecraft:entity.elder_guardian.curse", 522},
				{"minecraft:entity.elder_guardian.death", 523}, {"minecraft:entity.elder_guardian.death_land", 524}, {"minecraft:entity.elder_guardian.flop", 525}, {"minecraft:entity.elder_guardian.hurt", 526},
				{"minecraft:entity.elder_guardian.hurt_land", 527}, {"minecraft:entity.ender_dragon.ambient", 532}, {"minecraft:entity.ender_dragon.death", 533}, {"minecraft:entity.ender_dragon.flap", 535},
				{"minecraft:entity.ender_dragon.growl", 536}, {"minecraft:entity.ender_dragon.hurt", 537}, {"minecraft:entity.ender_dragon.shoot", 538}, {"minecraft:entity.ender_eye.death", 539},
				{"minecraft:entity.ender_eye.launch", 540}, {"minecraft:entity.ender_pearl.throw", 551}, {"minecraft:entity.enderman.ambient", 541}, {"minecraft:entity.enderman.death", 542},
				{"minecraft:entity.enderman.hurt", 543}, {"minecraft:entity.enderman.scream", 544}, {"minecraft:entity.enderman.stare", 545}, {"minecraft:entity.enderman.teleport", 546},
				{"minecraft:entity.endermite.ambient", 547}, {"minecraft:entity.endermite.death", 548}, {"minecraft:entity.endermite.hurt", 549}, {"minecraft:entity.endermite.step", 550},
				{"minecraft:entity.evoker.ambient", 555}, {"minecraft:entity.evoker.cast_spell", 556}, {"minecraft:entity.evoker.celebrate", 557}, {"minecraft:entity.evoker.death", 558},
				{"minecraft:entity.evoker.hurt", 560}, {"minecraft:entity.evoker.prepare_attack", 561}, {"minecraft:entity.evoker.prepare_summon", 562}, {"minecraft:entity.evoker.prepare_wololo", 563},
				{"minecraft:entity.evoker_fangs.attack", 559}, {"minecraft:entity.experience_bottle.throw", 564}, {"minecraft:entity.experience_orb.pickup", 565}, {"minecraft:entity.firework_rocket.blast", 575},
				{"minecraft:entity.firework_rocket.blast_far", 576}, {"minecraft:entity.firework_rocket.large_blast", 577}, {"minecraft:entity.firework_rocket.large_blast_far", 578}, {"minecraft:entity.firework_rocket.launch", 579},
				{"minecraft:entity.firework_rocket.shoot", 580}, {"minecraft:entity.firework_rocket.twinkle", 581}, {"minecraft:entity.firework_rocket.twinkle_far", 582}, {"minecraft:entity.fish.swim", 585},
				{"minecraft:entity.fishing_bobber.retrieve", 586}, {"minecraft:entity.fishing_bobber.splash", 587}, {"minecraft:entity.fishing_bobber.throw", 588}, {"minecraft:entity.fox.aggro", 595},
				{"minecraft:entity.fox.ambient", 596}, {"minecraft:entity.fox.bite", 597}, {"minecraft:entity.fox.death", 598}, {"minecraft:entity.fox.eat", 599},
				{"minecraft:entity.fox.hurt", 600}, {"minecraft:entity.fox.screech", 601}, {"minecraft:entity.fox.sleep", 602}, {"minecraft:entity.fox.sniff", 603},
				{"minecraft:entity.fox.spit", 604}, {"minecraft:entity.fox.teleport", 605}, {"minecraft:entity.frog.ambient", 627}, {"minecraft:entity.frog.death", 628},
				{"minecraft:entity.frog.eat", 629}, {"minecraft:entity.frog.hurt", 630}, {"minecraft:entity.frog.lay_spawn", 631}, {"minecraft:entity.frog.long_jump", 632},
				{"minecraft:entity.frog.step", 633}, {"minecraft:entity.frog.tongue", 634}, {"minecraft:entity.generic.big_fall", 641}, {"minecraft:entity.generic.burn", 642},
				{"minecraft:entity.generic.death", 643}, {"minecraft:entity.generic.drink", 644}, {"minecraft:entity.generic.eat", 645}, {"minecraft:entity.generic.explode", 646},
				{"minecraft:entity.generic.extinguish_fire", 647}, {"minecraft:entity.generic.hurt", 648}, {"minecraft:entity.generic.small_fall", 649}, {"minecraft:entity.generic.splash", 650},
				{"minecraft:entity.generic.swim", 651}, {"minecraft:entity.ghast.ambient", 652}, {"minecraft:entity.ghast.death", 653}, {"minecraft:entity.ghast.hurt", 654},
				{"minecraft:entity.ghast.scream", 655}, {"minecraft:entity.ghast.shoot", 656}, {"minecraft:entity.ghast.warn", 657}, {"minecraft:entity.ghastling.ambient", 658},
				{"minecraft:entity.ghastling.death", 659}, {"minecraft:entity.ghastling.hurt", 660}, {"minecraft:entity.ghastling.spawn", 661}, {"minecraft:entity.glow_item_frame.add_item", 673},
				{"minecraft:entity.glow_item_frame.break", 674}, {"minecraft:entity.glow_item_frame.place", 675}, {"minecraft:entity.glow_item_frame.remove_item", 676}, {"minecraft:entity.glow_item_frame.rotate_item", 677},
				{"minecraft:entity.glow_squid.ambient", 678}, {"minecraft:entity.glow_squid.death", 679}, {"minecraft:entity.glow_squid.hurt", 680}, {"minecraft:entity.glow_squid.squirt", 681},
				{"minecraft:entity.goat.ambient", 682}, {"minecraft:entity.goat.death", 683}, {"minecraft:entity.goat.eat", 684}, {"minecraft:entity.goat.horn_break", 690},
				{"minecraft:entity.goat.hurt", 685}, {"minecraft:entity.goat.long_jump", 686}, {"minecraft:entity.goat.milk", 687}, {"minecraft:entity.goat.prepare_ram", 688},
				{"minecraft:entity.goat.ram_impact", 689}, {"minecraft:entity.goat.screaming.ambient", 691}, {"minecraft:entity.goat.screaming.death", 692}, {"minecraft:entity.goat.screaming.eat", 693},
				{"minecraft:entity.goat.screaming.hurt", 694}, {"minecraft:entity.goat.screaming.long_jump", 695}, {"minecraft:entity.goat.screaming.milk", 696}, {"minecraft:entity.goat.screaming.prepare_ram", 697},
				{"minecraft:entity.goat.screaming.ram_impact", 698}, {"minecraft:entity.goat.step", 699}, {"minecraft:entity.guardian.ambient", 712}, {"minecraft:entity.guardian.ambient_land", 713},
				{"minecraft:entity.guardian.attack", 714}, {"minecraft:entity.guardian.death", 715}, {"minecraft:entity.guardian.death_land", 716}, {"minecraft:entity.guardian.flop", 717},
				{"minecraft:entity.guardian.hurt", 718}, {"minecraft:entity.guardian.hurt_land", 719}, {"minecraft:entity.happy_ghast.ambient", 730}, {"minecraft:entity.happy_ghast.death", 731},
				{"minecraft:entity.happy_ghast.equip", 765}, {"minecraft:entity.happy_ghast.harness_goggles_down", 768}, {"minecraft:entity.happy_ghast.harness_goggles_up", 767}, {"minecraft:entity.happy_ghast.hurt", 732},
				{"minecraft:entity.happy_ghast.riding", 733}, {"minecraft:entity.happy_ghast.unequip", 766}, {"minecraft:entity.hoglin.ambient", 770}, {"minecraft:entity.hoglin.angry", 771},
				{"minecraft:entity.hoglin.attack", 772}, {"minecraft:entity.hoglin.converted_to_zombified", 773}, {"minecraft:entity.hoglin.death", 774}, {"minecraft:entity.hoglin.hurt", 775},
				{"minecraft:entity.hoglin.retreat", 776}, {"minecraft:entity.hoglin.step", 777}, {"minecraft:entity.horse.ambient", 794}, {"minecraft:entity.horse.angry", 795},
				{"minecraft:entity.horse.armor", 796}, {"minecraft:entity.horse.breathe", 798}, {"minecraft:entity.horse.death", 799}, {"minecraft:entity.horse.eat", 800},
				{"minecraft:entity.horse.gallop", 801}, {"minecraft:entity.horse.hurt", 802}, {"minecraft:entity.horse.jump", 803}, {"minecraft:entity.horse.land", 804},
				{"minecraft:entity.horse.saddle", 805}, {"minecraft:entity.horse.step", 806}, {"minecraft:entity.horse.step_wood", 807}, {"minecraft:entity.hostile.big_fall", 808},
				{"minecraft:entity.hostile.death", 809}, {"minecraft:entity.hostile.hurt", 810}, {"minecraft:entity.hostile.small_fall", 811}, {"minecraft:entity.hostile.splash", 812},
				{"minecraft:entity.hostile.swim", 813}, {"minecraft:entity.husk.ambient", 814}, {"minecraft:entity.husk.converted_to_zombie", 815}, {"minecraft:entity.husk.death", 816},
				{"minecraft:entity.husk.hurt", 817}, {"minecraft:entity.husk.step", 818}, {"minecraft:entity.illusioner.ambient", 819}, {"minecraft:entity.illusioner.cast_spell", 820},
				{"minecraft:entity.illusioner.death", 821}, {"minecraft:entity.illusioner.hurt", 822}, {"minecraft:entity.illusioner.mirror_move", 823}, {"minecraft:entity.illusioner.prepare_blindness", 824},
				{"minecraft:entity.illusioner.prepare_mirror", 825}, {"minecraft:entity.iron_golem.attack", 834}, {"minecraft:entity.iron_golem.damage", 835}, {"minecraft:entity.iron_golem.death", 836},
				{"minecraft:entity.iron_golem.hurt", 837}, {"minecraft:entity.iron_golem.repair", 838}, {"minecraft:entity.iron_golem.step", 839}, {"minecraft:entity.item.break", 847},
				{"minecraft:entity.item.pickup", 848}, {"minecraft:entity.item_frame.add_item", 842}, {"minecraft:entity.item_frame.break", 843}, {"minecraft:entity.item_frame.place", 844},
				{"minecraft:entity.item_frame.remove_item", 845}, {"minecraft:entity.item_frame.rotate_item", 846}, {"minecraft:entity.lightning_bolt.impact", 873}, {"minecraft:entity.lightning_bolt.thunder", 874},
				{"minecraft:entity.lingering_potion.throw", 875}, {"minecraft:entity.llama.ambient", 876}, {"minecraft:entity.llama.angry", 877}, {"minecraft:entity.llama.chest", 878},
				{"minecraft:entity.llama.death", 879}, {"minecraft:entity.llama.eat", 880}, {"minecraft:entity.llama.hurt", 881}, {"minecraft:entity.llama.spit", 882},
				{"minecraft:entity.llama.step", 883}, {"minecraft:entity.llama.swag", 884}, {"minecraft:entity.magma_cube.death", 896}, {"minecraft:entity.magma_cube.death_small", 886},
				{"minecraft:entity.magma_cube.hurt", 897}, {"minecraft:entity.magma_cube.hurt_small", 898}, {"minecraft:entity.magma_cube.jump", 899}, {"minecraft:entity.magma_cube.squish", 900},
				{"minecraft:entity.magma_cube.squish_small", 901}, {"minecraft:entity.minecart.inside", 917}, {"minecraft:entity.minecart.inside.underwater", 916}, {"minecraft:entity.minecart.riding", 918},
				{"minecraft:entity.mooshroom.convert", 919}, {"minecraft:entity.mooshroom.eat", 920}, {"minecraft:entity.mooshroom.milk", 921}, {"minecraft:entity.mooshroom.shear", 923},
				{"minecraft:entity.mooshroom.suspicious_milk", 922}, {"minecraft:entity.mule.ambient", 954}, {"minecraft:entity.mule.angry", 955}, {"minecraft:entity.mule.chest", 956},
				{"minecraft:entity.mule.death", 957}, {"minecraft:entity.mule.eat", 958}, {"minecraft:entity.mule.hurt", 959}, {"minecraft:entity.mule.jump", 960},
				{"minecraft:entity.ocelot.ambient", 1104}, {"minecraft:entity.ocelot.death", 1105}, {"minecraft:entity.ocelot.hurt", 1103}, {"minecraft:entity.painting.break", 1107},
				{"minecraft:entity.painting.place", 1108}, {"minecraft:entity.panda.aggressive_ambient", 1117}, {"minecraft:entity.panda.ambient", 1112}, {"minecraft:entity.panda.bite", 1120},
				{"minecraft:entity.panda.cant_breed", 1116}, {"minecraft:entity.panda.death", 1113}, {"minecraft:entity.panda.eat", 1114}, {"minecraft:entity.panda.hurt", 1119},
				{"minecraft:entity.panda.pre_sneeze", 1110}, {"minecraft:entity.panda.sneeze", 1111}, {"minecraft:entity.panda.step", 1115}, {"minecraft:entity.panda.worried_ambient", 1118},
				{"minecraft:entity.parrot.ambient", 1121}, {"minecraft:entity.parrot.death", 1122}, {"minecraft:entity.parrot.eat", 1123}, {"minecraft:entity.parrot.fly", 1124},
				{"minecraft:entity.parrot.hurt", 1125}, {"minecraft:entity.parrot.imitate.blaze", 1126}, {"minecraft:entity.parrot.imitate.bogged", 1127}, {"minecraft:entity.parrot.imitate.breeze", 1128},
				{"minecraft:entity.parrot.imitate.creaking", 1129}, {"minecraft:entity.parrot.imitate.creeper", 1130}, {"minecraft:entity.parrot.imitate.drowned", 1131}, {"minecraft:entity.parrot.imitate.elder_guardian", 1132},
				{"minecraft:entity.parrot.imitate.ender_dragon", 1133}, {"minecraft:entity.parrot.imitate.endermite", 1134}, {"minecraft:entity.parrot.imitate.evoker", 1135}, {"minecraft:entity.parrot.imitate.ghast", 1136},
				{"minecraft:entity.parrot.imitate.guardian", 1137}, {"minecraft:entity.parrot.imitate.hoglin", 1138}, {"minecraft:entity.parrot.imitate.husk", 1139}, {"minecraft:entity.parrot.imitate.illusioner", 1140},
				{"minecraft:entity.parrot.imitate.magma_cube", 1141}, {"minecraft:entity.parrot.imitate.phantom", 1142}, {"minecraft:entity.parrot.imitate.piglin", 1143}, {"minecraft:entity.parrot.imitate.piglin_brute", 1144},
				{"minecraft:entity.parrot.imitate.pillager", 1145}, {"minecraft:entity.parrot.imitate.ravager", 1146}, {"minecraft:entity.parrot.imitate.shulker", 1147}, {"minecraft:entity.parrot.imitate.silverfish", 1148},
				{"minecraft:entity.parrot.imitate.skeleton", 1149}, {"minecraft:entity.parrot.imitate.slime", 1150}, {"minecraft:entity.parrot.imitate.spider", 1151}, {"minecraft:entity.parrot.imitate.stray", 1152},
				{"minecraft:entity.parrot.imitate.vex", 1153}, {"minecraft:entity.parrot.imitate.vindicator", 1154}, {"minecraft:entity.parrot.imitate.warden", 1155}, {"minecraft:entity.parrot.imitate.witch", 1156},
				{"minecraft:entity.parrot.imitate.wither", 1157}, {"minecraft:entity.parrot.imitate.wither_skeleton", 1158}, {"minecraft:entity.parrot.imitate.zoglin", 1159}, {"minecraft:entity.parrot.imitate.zombie", 1160},
				{"minecraft:entity.parrot.imitate.zombie_villager", 1161}, {"minecraft:entity.parrot.step", 1162}, {"minecraft:entity.phantom.ambient", 1163}, {"minecraft:entity.phantom.bite", 1164},
				{"minecraft:entity.phantom.death", 1165}, {"minecraft:entity.phantom.flap", 1166}, {"minecraft:entity.phantom.hurt", 1167}, {"minecraft:entity.phantom.swoop", 1168},
				{"minecraft:entity.pig.ambient", 1169}, {"minecraft:entity.pig.death", 1170}, {"minecraft:entity.pig.hurt", 1171}, {"minecraft:entity.pig.saddle", 1172},
				{"minecraft:entity.pig.step", 1173}, {"minecraft:entity.piglin.admiring_item", 1174}, {"minecraft:entity.piglin.ambient", 1175}, {"minecraft:entity.piglin.angry", 1176},
				{"minecraft:entity.piglin.celebrate", 1177}, {"minecraft:entity.piglin.converted_to_zombified", 1183}, {"minecraft:entity.piglin.death", 1178}, {"minecraft:entity.piglin.hurt", 1180},
				{"minecraft:entity.piglin.jealous", 1179}, {"minecraft:entity.piglin.retreat", 1181}, {"minecraft:entity.piglin.step", 1182}, {"minecraft:entity.piglin_brute.ambient", 1184},
				{"minecraft:entity.piglin_brute.angry", 1185}, {"minecraft:entity.piglin_brute.converted_to_zombified", 1189}, {"minecraft:entity.piglin_brute.death", 1186}, {"minecraft:entity.piglin_brute.hurt", 1187},
				{"minecraft:entity.piglin_brute.step", 1188}, {"minecraft:entity.pillager.ambient", 1190}, {"minecraft:entity.pillager.celebrate", 1191}, {"minecraft:entity.pillager.death", 1192},
				{"minecraft:entity.pillager.hurt", 1193}, {"minecraft:entity.player.attack.crit", 1196}, {"minecraft:entity.player.attack.knockback", 1197}, {"minecraft:entity.player.attack.nodamage", 1198},
				{"minecraft:entity.player.attack.strong", 1199}, {"minecraft:entity.player.attack.sweep", 1200}, {"minecraft:entity.player.attack.weak", 1201}, {"minecraft:entity.player.big_fall", 1202},
				{"minecraft:entity.player.breath", 1203}, {"minecraft:entity.player.burp", 1204}, {"minecraft:entity.player.death", 1205}, {"minecraft:entity.player.hurt", 1206},
				{"minecraft:entity.player.hurt_drown", 1207}, {"minecraft:entity.player.hurt_freeze", 1208}, {"minecraft:entity.player.hurt_on_fire", 1209}, {"minecraft:entity.player.hurt_sweet_berry_bush", 1210},
				{"minecraft:entity.player.levelup", 1211}, {"minecraft:entity.player.small_fall", 1212}, {"minecraft:entity.player.splash", 1213}, {"minecraft:entity.player.splash.high_speed", 1214},
				{"minecraft:entity.player.swim", 1215}, {"minecraft:entity.player.teleport", 1216}, {"minecraft:entity.polar_bear.ambient", 1217}, {"minecraft:entity.polar_bear.ambient_baby", 1218},
				{"minecraft:entity.polar_bear.death", 1219}, {"minecraft:entity.polar_bear.hurt", 1220}, {"minecraft:entity.polar_bear.step", 1221}, {"minecraft:entity.polar_bear.warning", 1222},
				{"minecraft:entity.puffer_fish.blow_out", 1236}, {"minecraft:entity.puffer_fish.blow_up", 1237}, {"minecraft:entity.puffer_fish.death", 1238}, {"minecraft:entity.puffer_fish.flop", 1239},
				{"minecraft:entity.puffer_fish.hurt", 1240}, {"minecraft:entity.puffer_fish.sting", 1241}, {"minecraft:entity.rabbit.ambient", 1243}, {"minecraft:entity.rabbit.attack", 1244},
				{"minecraft:entity.rabbit.death", 1245}, {"minecraft:entity.rabbit.hurt", 1246}, {"minecraft:entity.rabbit.jump", 1247}, {"minecraft:entity.ravager.ambient", 1249},
				{"minecraft:entity.ravager.attack", 1250}, {"minecraft:entity.ravager.celebrate", 1251}, {"minecraft:entity.ravager.death", 1252}, {"minecraft:entity.ravager.hurt", 1253},
				{"minecraft:entity.ravager.roar", 1256}, {"minecraft:entity.ravager.step", 1254}, {"minecraft:entity.ravager.stunned", 1255}, {"minecraft:entity.salmon.ambient", 1277},
				{"minecraft:entity.salmon.death", 1278}, {"minecraft:entity.salmon.flop", 1279}, {"minecraft:entity.salmon.hurt", 1280}, {"minecraft:entity.sheep.ambient", 1323},
				{"minecraft:entity.sheep.death", 1324}, {"minecraft:entity.sheep.hurt", 1325}, {"minecraft:entity.sheep.shear", 1326}, {"minecraft:entity.sheep.step", 1327},
				{"minecraft:entity.shulker.ambient", 1348}, {"minecraft:entity.shulker.close", 1353}, {"minecraft:entity.shulker.death", 1354}, {"minecraft:entity.shulker.hurt", 1355},
				{"minecraft:entity.shulker.hurt_closed", 1356}, {"minecraft:entity.shulker.open", 1357}, {"minecraft:entity.shulker.shoot", 1358}, {"minecraft:entity.shulker.teleport", 1359},
				{"minecraft:entity.shulker_bullet.hit", 1351}, {"minecraft:entity.shulker_bullet.hurt", 1352}, {"minecraft:entity.silverfish.ambient", 1360}, {"minecraft:entity.silverfish.death", 1361},
				{"minecraft:entity.silverfish.hurt", 1362}, {"minecraft:entity.silverfish.step", 1363}, {"minecraft:entity.skeleton.ambient", 1364}, {"minecraft:entity.skeleton.converted_to_stray", 1365},
				{"minecraft:entity.skeleton.death", 1366}, {"minecraft:entity.skeleton.hurt", 1375}, {"minecraft:entity.skeleton.shoot", 1376}, {"minecraft:entity.skeleton.step", 1377},
				{"minecraft:entity.skeleton_horse.ambient", 1367}, {"minecraft:entity.skeleton_horse.ambient_water", 1371}, {"minecraft:entity.skeleton_horse.death", 1368}, {"minecraft:entity.skeleton_horse.gallop_water", 1372},
				{"minecraft:entity.skeleton_horse.hurt", 1369}, {"minecraft:entity.skeleton_horse.jump_water", 1373}, {"minecraft:entity.skeleton_horse.step_water", 1374}, {"minecraft:entity.skeleton_horse.swim", 1370},
				{"minecraft:entity.slime.attack", 1378}, {"minecraft:entity.slime.death", 1379}, {"minecraft:entity.slime.death_small", 1434}, {"minecraft:entity.slime.hurt", 1380},
				{"minecraft:entity.slime.hurt_small", 1435}, {"minecraft:entity.slime.jump", 1381}, {"minecraft:entity.slime.jump_small", 1436}, {"minecraft:entity.slime.squish", 1382},
				{"minecraft:entity.slime.squish_small", 1437}, {"minecraft:entity.sniffer.death", 1444}, {"minecraft:entity.sniffer.digging", 1449}, {"minecraft:entity.sniffer.digging_stop", 1450},
				{"minecraft:entity.sniffer.drop_seed", 1445}, {"minecraft:entity.sniffer.eat", 1441}, {"minecraft:entity.sniffer.happy", 1451}, {"minecraft:entity.sniffer.hurt", 1443},
				{"minecraft:entity.sniffer.idle", 1442}, {"minecraft:entity.sniffer.scenting", 1446}, {"minecraft:entity.sniffer.searching", 1448}, {"minecraft:entity.sniffer.sniffing", 1447},
				{"minecraft:entity.sniffer.step", 1440}, {"minecraft:entity.snow_golem.ambient", 1458}, {"minecraft:entity.snow_golem.death", 1459}, {"minecraft:entity.snow_golem.hurt", 1460},
				{"minecraft:entity.snow_golem.shear", 1462}, {"minecraft:entity.snow_golem.shoot", 1461}, {"minecraft:entity.snowball.throw", 1455}, {"minecraft:entity.spider.ambient", 1466},
				{"minecraft:entity.spider.death", 1467}, {"minecraft:entity.spider.hurt", 1468}, {"minecraft:entity.spider.step", 1469}, {"minecraft:entity.splash_potion.break", 1470},
				{"minecraft:entity.splash_potion.throw", 1471}, {"minecraft:entity.squid.ambient", 1480}, {"minecraft:entity.squid.death", 1481}, {"minecraft:entity.squid.hurt", 1482},
				{"minecraft:entity.squid.squirt", 1483}, {"minecraft:entity.stray.ambient", 1493}, {"minecraft:entity.stray.death", 1494}, {"minecraft:entity.stray.hurt", 1495},
				{"minecraft:entity.stray.step", 1496}, {"minecraft:entity.strider.ambient", 1425}, {"minecraft:entity.strider.death", 1428}, {"minecraft:entity.strider.eat", 1432},
				{"minecraft:entity.strider.happy", 1426}, {"minecraft:entity.strider.hurt", 1429}, {"minecraft:entity.strider.retreat", 1427}, {"minecraft:entity.strider.saddle", 1433},
				{"minecraft:entity.strider.step", 1430}, {"minecraft:entity.strider.step_lava", 1431}, {"minecraft:entity.tadpole.death", 1500}, {"minecraft:entity.tadpole.flop", 1501},
				{"minecraft:entity.tadpole.grow_up", 1502}, {"minecraft:entity.tadpole.hurt", 1503}, {"minecraft:entity.tnt.primed", 1505}, {"minecraft:entity.tropical_fish.ambient", 1519},
				{"minecraft:entity.tropical_fish.death", 1520}, {"minecraft:entity.tropical_fish.flop", 1521}, {"minecraft:entity.tropical_fish.hurt", 1522}, {"minecraft:entity.turtle.ambient_land", 1538},
				{"minecraft:entity.turtle.death", 1539}, {"minecraft:entity.turtle.death_baby", 1540}, {"minecraft:entity.turtle.egg_break", 1541}, {"minecraft:entity.turtle.egg_crack", 1542},
				{"minecraft:entity.turtle.egg_hatch", 1543}, {"minecraft:entity.turtle.hurt", 1544}, {"minecraft:entity.turtle.hurt_baby", 1545}, {"minecraft:entity.turtle.lay_egg", 1546},
				{"minecraft:entity.turtle.shamble", 1547}, {"minecraft:entity.turtle.shamble_baby", 1548}, {"minecraft:entity.turtle.swim", 1549}, {"minecraft:entity.vex.ambient", 1573},
				{"minecraft:entity.vex.charge", 1574}, {"minecraft:entity.vex.death", 1575}, {"minecraft:entity.vex.hurt", 1576}, {"minecraft:entity.villager.ambient", 1577},
				{"minecraft:entity.villager.celebrate", 1578}, {"minecraft:entity.villager.death", 1579}, {"minecraft:entity.villager.hurt", 1580}, {"minecraft:entity.villager.no", 1581},
				{"minecraft:entity.villager.trade", 1582}, {"minecraft:entity.villager.work_armorer", 1584}, {"minecraft:entity.villager.work_butcher", 1585}, {"minecraft:entity.villager.work_cartographer", 1586},
				{"minecraft:entity.villager.work_cleric", 1587}, {"minecraft:entity.villager.work_farmer", 1588}, {"minecraft:entity.villager.work_fisherman", 1589}, {"minecraft:entity.villager.work_fletcher", 1590},
				{"minecraft:entity.villager.work_leatherworker", 1591}, {"minecraft:entity.villager.work_librarian", 1592}, {"minecraft:entity.villager.work_mason", 1593}, {"minecraft:entity.villager.work_shepherd", 1594},
				{"minecraft:entity.villager.work_toolsmith", 1595}, {"minecraft:entity.villager.work_weaponsmith", 1596}, {"minecraft:entity.villager.yes", 1583}, {"minecraft:entity.vindicator.ambient", 1597},
				{"minecraft:entity.vindicator.celebrate", 1598}, {"minecraft:entity.vindicator.death", 1599}, {"minecraft:entity.vindicator.hurt", 1600}, {"minecraft:entity.wandering_trader.ambient", 1607},
				{"minecraft:entity.wandering_trader.death", 1608}, {"minecraft:entity.wandering_trader.disappeared", 1609}, {"minecraft:entity.wandering_trader.drink_milk", 1610}, {"minecraft:entity.wandering_trader.drink_potion", 1611},
				{"minecraft:entity.wandering_trader.hurt", 1612}, {"minecraft:entity.wandering_trader.no", 1613}, {"minecraft:entity.wandering_trader.reappeared", 1614}, {"minecraft:entity.wandering_trader.trade", 1615},
				{"minecraft:entity.wandering_trader.yes", 1616}, {"minecraft:entity.warden.agitated", 1617}, {"minecraft:entity.warden.ambient", 1618}, {"minecraft:entity.warden.angry", 1619},
				{"minecraft:entity.warden.attack_impact", 1620}, {"minecraft:entity.warden.death", 1621}, {"minecraft:entity.warden.dig", 1622}, {"minecraft:entity.warden.emerge", 1623},
				{"minecraft:entity.warden.heartbeat", 1624}, {"minecraft:entity.warden.hurt", 1625}, {"minecraft:entity.warden.listening", 1626}, {"minecraft:entity.warden.listening_angry", 1627},
				{"minecraft:entity.warden.nearby_close", 1628}, {"minecraft:entity.warden.nearby_closer", 1629}, {"minecraft:entity.warden.nearby_closest", 1630}, {"minecraft:entity.warden.roar", 1631},
				{"minecraft:entity.warden.sniff", 1632}, {"minecraft:entity.warden.sonic_boom", 1633}, {"minecraft:entity.warden.sonic_charge", 1634}, {"minecraft:entity.warden.step", 1635},
				{"minecraft:entity.warden.tendril_clicks", 1636}, {"minecraft:entity.wind_charge.throw", 1655}, {"minecraft:entity.wind_charge.wind_burst", 1654}, {"minecraft:entity.witch.ambient", 1656},
				{"minecraft:entity.witch.celebrate", 1657}, {"minecraft:entity.witch.death", 1658}, {"minecraft:entity.witch.drink", 1659}, {"minecraft:entity.witch.hurt", 1660},
				{"minecraft:entity.witch.throw", 1661}, {"minecraft:entity.wither.ambient", 1662}, {"minecraft:entity.wither.break_block", 1663}, {"minecraft:entity.wither.death", 1664},
				{"minecraft:entity.wither.hurt", 1665}, {"minecraft:entity.wither.shoot", 1666}, {"minecraft:entity.wither.spawn", 1671}, {"minecraft:entity.wither_skeleton.ambient", 1667},
				{"minecraft:entity.wither_skeleton.death", 1668}, {"minecraft:entity.wither_skeleton.hurt", 1669}, {"minecraft:entity.wither_skeleton.step", 1670}, {"minecraft:entity.wolf.ambient", 1678},
				{"minecraft:entity.wolf.death", 1679}, {"minecraft:entity.wolf.growl", 1680}, {"minecraft:entity.wolf.hurt", 1681}, {"minecraft:entity.wolf.pant", 1682},
				{"minecraft:entity.wolf.shake", 1676}, {"minecraft:entity.wolf.step", 1677}, {"minecraft:entity.wolf.whine", 1683}, {"minecraft:entity.wolf_angry.ambient", 1696},
				{"minecraft:entity.wolf_angry.death", 1697}, {"minecraft:entity.wolf_angry.growl", 1698}, {"minecraft:entity.wolf_angry.hurt", 1699}, {"minecraft:entity.wolf_angry.pant", 1700},
				{"minecraft:entity.wolf_angry.whine", 1701}, {"minecraft:entity.wolf_big.ambient", 1708}, {"minecraft:entity.wolf_big.death", 1709}, {"minecraft:entity.wolf_big.growl", 1710},
				{"minecraft:entity.wolf_big.hurt", 1711}, {"minecraft:entity.wolf_big.pant", 1712}, {"minecraft:entity.wolf_big.whine", 1713}, {"minecraft:entity.wolf_cute.ambient", 1714},
				{"minecraft:entity.wolf_cute.death", 1715}, {"minecraft:entity.wolf_cute.growl", 1716}, {"minecraft:entity.wolf_cute.hurt", 1717}, {"minecraft:entity.wolf_cute.pant", 1718},
				{"minecraft:entity.wolf_cute.whine", 1719}, {"minecraft:entity.wolf_grumpy.ambient", 1702}, {"minecraft:entity.wolf_grumpy.death", 1703}, {"minecraft:entity.wolf_grumpy.growl", 1704},
				{"minecraft:entity.wolf_grumpy.hurt", 1705}, {"minecraft:entity.wolf_grumpy.pant", 1706}, {"minecraft:entity.wolf_grumpy.whine", 1707}, {"minecraft:entity.wolf_puglin.ambient", 1684},
				{"minecraft:entity.wolf_puglin.death", 1685}, {"minecraft:entity.wolf_puglin.growl", 1686}, {"minecraft:entity.wolf_puglin.hurt", 1687}, {"minecraft:entity.wolf_puglin.pant", 1688},
				{"minecraft:entity.wolf_puglin.whine", 1689}, {"minecraft:entity.wolf_sad.ambient", 1690}, {"minecraft:entity.wolf_sad.death", 1691}, {"minecraft:entity.wolf_sad.growl", 1692},
				{"minecraft:entity.wolf_sad.hurt", 1693}, {"minecraft:entity.wolf_sad.pant", 1694}, {"minecraft:entity.wolf_sad.whine", 1695}, {"minecraft:entity.zoglin.ambient", 1738},
				{"minecraft:entity.zoglin.angry", 1739}, {"minecraft:entity.zoglin.attack", 1740}, {"minecraft:entity.zoglin.death", 1741}, {"minecraft:entity.zoglin.hurt", 1742},
				{"minecraft:entity.zoglin.step", 1743}, {"minecraft:entity.zombie.ambient", 1744}, {"minecraft:entity.zombie.attack_iron_door", 1746}, {"minecraft:entity.zombie.attack_wooden_door", 1745},
				{"minecraft:entity.zombie.break_wooden_door", 1747}, {"minecraft:entity.zombie.converted_to_drowned", 1748}, {"minecraft:entity.zombie.death", 1749}, {"minecraft:entity.zombie.destroy_egg", 1750},
				{"minecraft:entity.zombie.hurt", 1754}, {"minecraft:entity.zombie.infect", 1755}, {"minecraft:entity.zombie.step", 1760}, {"minecraft:entity.zombie_horse.ambient", 1751},
				{"minecraft:entity.zombie_horse.death", 1752}, {"minecraft:entity.zombie_horse.hurt", 1753}, {"minecraft:entity.zombie_villager.ambient", 1761}, {"minecraft:entity.zombie_villager.converted", 1762},
				{"minecraft:entity.zombie_villager.cure", 1763}, {"minecraft:entity.zombie_villager.death", 1764}, {"minecraft:entity.zombie_villager.hurt", 1765}, {"minecraft:entity.zombie_villager.step", 1766},
				{"minecraft:entity.zombified_piglin.ambient", 1756}, {"minecraft:entity.zombified_piglin.angry", 1757}, {"minecraft:entity.zombified_piglin.death", 1758}, {"minecraft:entity.zombified_piglin.hurt", 1759},
				{"minecraft:event.mob_effect.bad_omen", 1767}, {"minecraft:event.mob_effect.raid_omen", 1769}, {"minecraft:event.mob_effect.trial_omen", 1768}, {"minecraft:event.raid.horn", 1248},
				{"minecraft:intentionally_empty", 1035}, {"minecraft:item.armor.equip_chain", 67}, {"minecraft:item.armor.equip_copper", 74}, {"minecraft:item.armor.equip_diamond", 68},
				{"minecraft:item.armor.equip_elytra", 69}, {"minecraft:item.armor.equip_generic", 70}, {"minecraft:item.armor.equip_gold", 71}, {"minecraft:item.armor.equip_iron", 72},
				{"minecraft:item.armor.equip_leather", 73}, {"minecraft:item.armor.equip_netherite", 75}, {"minecraft:item.armor.equip_turtle", 76}, {"minecraft:item.armor.equip_wolf", 77},
				{"minecraft:item.armor.unequip_wolf", 78}, {"minecraft:item.axe.scrape", 87}, {"minecraft:item.axe.strip", 86}, {"minecraft:item.axe.wax_off", 88},
				{"minecraft:item.bone_meal.use", 180}, {"minecraft:item.book.page_turn", 181}, {"minecraft:item.book.put", 182}, {"minecraft:item.bottle.empty", 184},
				{"minecraft:item.bottle.fill", 185}, {"minecraft:item.bottle.fill_dragonbreath", 186}, {"minecraft:item.brush.brushing.generic", 201}, {"minecraft:item.brush.brushing.gravel", 203},
				{"minecraft:item.brush.brushing.gravel.complete", 205}, {"minecraft:item.brush.brushing.sand", 202}, {"minecraft:item.brush.brushing.sand.complete", 204}, {"minecraft:item.bucket.empty", 212},
				{"minecraft:item.bucket.empty_axolotl", 213}, {"minecraft:item.bucket.empty_fish", 214}, {"minecraft:item.bucket.empty_lava", 215}, {"minecraft:item.bucket.empty_powder_snow", 216},
				{"minecraft:item.bucket.empty_tadpole", 217}, {"minecraft:item.bucket.fill", 218}, {"minecraft:item.bucket.fill_axolotl", 219}, {"minecraft:item.bucket.fill_fish", 220},
				{"minecraft:item.bucket.fill_lava", 221}, {"minecraft:item.bucket.fill_powder_snow", 222}, {"minecraft:item.bucket.fill_tadpole", 223}, {"minecraft:item.bundle.drop_contents", 224},
				{"minecraft:item.bundle.insert", 225}, {"minecraft:item.bundle.insert_fail", 226}, {"minecraft:item.bundle.remove_one", 227}, {"minecraft:item.chorus_fruit.teleport", 324},
				{"minecraft:item.crop.plant", 430}, {"minecraft:item.crossbow.hit", 431}, {"minecraft:item.crossbow.loading_end", 432}, {"minecraft:item.crossbow.loading_middle", 433},
				{"minecraft:item.crossbow.loading_start", 434}, {"minecraft:item.crossbow.quick_charge_1", 435}, {"minecraft:item.crossbow.quick_charge_2", 436}, {"minecraft:item.crossbow.quick_charge_3", 437},
				{"minecraft:item.crossbow.shoot", 438}, {"minecraft:item.dye.use", 518}, {"minecraft:item.elytra.flying", 528}, {"minecraft:item.firecharge.use", 573},
				{"minecraft:item.flintandsteel.use", 589}, {"minecraft:item.glow_ink_sac.use", 672}, {"minecraft:item.goat_horn.sound.0", 786}, {"minecraft:item.goat_horn.sound.1", 787},
				{"minecraft:item.goat_horn.sound.2", 788}, {"minecraft:item.goat_horn.sound.3", 789}, {"minecraft:item.goat_horn.sound.4", 790}, {"minecraft:item.goat_horn.sound.5", 791},
				{"minecraft:item.goat_horn.sound.6", 792}, {"minecraft:item.goat_horn.sound.7", 793}, {"minecraft:item.hoe.till", 769}, {"minecraft:item.honey_bottle.drink", 785},
				{"minecraft:item.honeycomb.wax_on", 784}, {"minecraft:item.horse_armor.unequip", 797}, {"minecraft:item.ink_sac.use", 826}, {"minecraft:item.lead.break", 871},
				{"minecraft:item.lead.tied", 870}, {"minecraft:item.lead.untied", 869}, {"minecraft:item.llama_carpet.unequip", 885}, {"minecraft:item.lodestone_compass.lock", 892},
				{"minecraft:item.mace.smash_air", 893}, {"minecraft:item.mace.smash_ground", 894}, {"minecraft:item.mace.smash_ground_heavy", 895}, {"minecraft:item.nether_wart.plant", 1019},
				{"minecraft:item.ominous_bottle.dispose", 1106}, {"minecraft:item.saddle.unequip", 1770}, {"minecraft:item.shears.snip", 1328}, {"minecraft:item.shield.block", 1340},
				{"minecraft:item.shield.break", 1341}, {"minecraft:item.shovel.flatten", 1347}, {"minecraft:item.spyglass.stop_using", 1479}, {"minecraft:item.spyglass.use", 1478},
				{"minecraft:item.totem.use", 1506}, {"minecraft:item.trident.hit", 1507}, {"minecraft:item.trident.hit_ground", 1508}, {"minecraft:item.trident.return", 1509},
				{"minecraft:item.trident.riptide_1", 1510}, {"minecraft:item.trident.riptide_2", 1511}, {"minecraft:item.trident.riptide_3", 1512}, {"minecraft:item.trident.throw", 1513},
				{"minecraft:item.trident.thunder", 1514}, {"minecraft:item.wolf_armor.break", 1672}, {"minecraft:item.wolf_armor.crack", 1673}, {"minecraft:item.wolf_armor.damage", 1674},
				{"minecraft:item.wolf_armor.repair", 1675}, {"minecraft:music.creative", 961}, {"minecraft:music.credits", 962}, {"minecraft:music.dragon", 984},
				{"minecraft:music.end", 985}, {"minecraft:music.game", 986}, {"minecraft:music.menu", 987}, {"minecraft:music.nether.basalt_deltas", 988},
				{"minecraft:music.nether.crimson_forest", 989}, {"minecraft:music.nether.nether_wastes", 1000}, {"minecraft:music.nether.soul_sand_valley", 1003}, {"minecraft:music.nether.warped_forest", 1005},
				{"minecraft:music.overworld.badlands", 1008}, {"minecraft:music.overworld.bamboo_jungle", 1011}, {"minecraft:music.overworld.cherry_grove", 999}, {"minecraft:music.overworld.deep_dark", 990},
				{"minecraft:music.overworld.desert", 1007}, {"minecraft:music.overworld.dripstone_caves", 991}, {"minecraft:music.overworld.flower_forest", 1006}, {"minecraft:music.overworld.forest", 996},
				{"minecraft:music.overworld.frozen_peaks", 1001}, {"minecraft:music.overworld.grove", 992}, {"minecraft:music.overworld.jagged_peaks", 993}, {"minecraft:music.overworld.jungle", 1009},
				{"minecraft:music.overworld.lush_caves", 994}, {"minecraft:music.overworld.meadow", 998}, {"minecraft:music.overworld.old_growth_taiga", 997}, {"minecraft:music.overworld.snowy_slopes", 1002},
				{"minecraft:music.overworld.sparse_jungle", 1010}, {"minecraft:music.overworld.stony_peaks", 1004}, {"minecraft:music.overworld.swamp", 995}, {"minecraft:music.under_water", 1012},
				{"minecraft:music_disc.11", 964}, {"minecraft:music_disc.13", 965}, {"minecraft:music_disc.5", 963}, {"minecraft:music_disc.blocks", 966},
				{"minecraft:music_disc.cat", 967}, {"minecraft:music_disc.chirp", 968}, {"minecraft:music_disc.creator", 980}, {"minecraft:music_disc.creator_music_box", 981},
				{"minecraft:music_disc.far", 969}, {"minecraft:music_disc.lava_chicken", 970}, {"minecraft:music_disc.mall", 971}, {"minecraft:music_disc.mellohi", 972},
				{"minecraft:music_disc.otherside", 978}, {"minecraft:music_disc.pigstep", 973}, {"minecraft:music_disc.precipice", 982}, {"minecraft:music_disc.relic", 979},
				{"minecraft:music_disc.stal", 974}, {"minecraft:music_disc.strad", 975}, {"minecraft:music_disc.tears", 983}, {"minecraft:music_disc.wait", 976},
				{"minecraft:music_disc.ward", 977}, {"minecraft:particle.soul_escape", 1405}, {"minecraft:ui.button.click", 1550}, {"minecraft:ui.cartography_table.take_result", 1553},
				{"minecraft:ui.hud.bubble_pop", 211}, {"minecraft:ui.loom.select_pattern", 1551}, {"minecraft:ui.loom.take_result", 1552}, {"minecraft:ui.stonecutter.select_recipe", 1555},
				{"minecraft:ui.stonecutter.take_result", 1554}, {"minecraft:ui.toast.challenge_complete", 1556}, {"minecraft:ui.toast.in", 1557}, {"minecraft:ui.toast.out", 1558},
				{"minecraft:weather.end_flash", 1640}, {"minecraft:weather.rain", 1641}, {"minecraft:weather.rain.above", 1642}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getSpawnConditionType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:biome", 2}, {"minecraft:moon_brightness", 1}, {"minecraft:structure", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getStatType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:broken", 3}, {"minecraft:crafted", 1}, {"minecraft:custom", 8}, {"minecraft:dropped", 5},
				{"minecraft:killed", 6}, {"minecraft:killed_by", 7}, {"minecraft:mined", 0}, {"minecraft:picked_up", 4},
				{"minecraft:used", 2}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getTestEnvironmentDefinitionType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:all_of", 0}, {"minecraft:function", 4}, {"minecraft:game_rules", 1}, {"minecraft:time_of_day", 2},
				{"minecraft:weather", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getTestFunction() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:always_pass", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getTestInstanceType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:block_based", 0}, {"minecraft:function", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getTicketType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:dragon", 2}, {"minecraft:ender_pearl", 7}, {"minecraft:forced", 5}, {"minecraft:player_loading", 3},
				{"minecraft:player_simulation", 4}, {"minecraft:player_spawn", 0}, {"minecraft:portal", 6}, {"minecraft:spawn_search", 1},
				{"minecraft:unknown", 8}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getTriggerType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:allay_drop_item_on_block", 52}, {"minecraft:any_block_use", 40}, {"minecraft:avoid_vibration", 53}, {"minecraft:bee_nest_destroyed", 36},
				{"minecraft:bred_animals", 14}, {"minecraft:brewed_potion", 10}, {"minecraft:changed_dimension", 21}, {"minecraft:channeled_lightning", 30},
				{"minecraft:construct_beacon", 11}, {"minecraft:consume_item", 25}, {"minecraft:crafter_recipe_crafted", 55}, {"minecraft:cured_zombie_villager", 17},
				{"minecraft:default_block_use", 39}, {"minecraft:effects_changed", 26}, {"minecraft:enchanted_item", 8}, {"minecraft:enter_block", 3},
				{"minecraft:entity_hurt_player", 7}, {"minecraft:entity_killed_player", 2}, {"minecraft:fall_after_explosion", 56}, {"minecraft:fall_from_height", 49},
				{"minecraft:filled_bucket", 9}, {"minecraft:fishing_rod_hooked", 29}, {"minecraft:hero_of_the_village", 33}, {"minecraft:impossible", 0},
				{"minecraft:inventory_changed", 4}, {"minecraft:item_durability_changed", 19}, {"minecraft:item_used_on_block", 38}, {"minecraft:kill_mob_near_sculk_catalyst", 51},
				{"minecraft:killed_by_arrow", 32}, {"minecraft:levitation", 20}, {"minecraft:lightning_strike", 47}, {"minecraft:location", 15},
				{"minecraft:nether_travel", 28}, {"minecraft:placed_block", 24}, {"minecraft:player_generates_container_loot", 41}, {"minecraft:player_hurt_entity", 6},
				{"minecraft:player_interacted_with_entity", 44}, {"minecraft:player_killed_entity", 1}, {"minecraft:player_sheared_equipment", 45}, {"minecraft:recipe_crafted", 54},
				{"minecraft:recipe_unlocked", 5}, {"minecraft:ride_entity_in_lava", 50}, {"minecraft:shot_crossbow", 31}, {"minecraft:slept_in_bed", 16},
				{"minecraft:slide_down_block", 35}, {"minecraft:started_riding", 46}, {"minecraft:summoned_entity", 13}, {"minecraft:tame_animal", 23},
				{"minecraft:target_hit", 37}, {"minecraft:thrown_item_picked_up_by_entity", 42}, {"minecraft:thrown_item_picked_up_by_player", 43}, {"minecraft:tick", 22},
				{"minecraft:used_ender_eye", 12}, {"minecraft:used_totem", 27}, {"minecraft:using_item", 48}, {"minecraft:villager_trade", 18},
				{"minecraft:voluntary_exile", 34}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getVillagerProfession() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:armorer", 1}, {"minecraft:butcher", 2}, {"minecraft:cartographer", 3}, {"minecraft:cleric", 4},
				{"minecraft:farmer", 5}, {"minecraft:fisherman", 6}, {"minecraft:fletcher", 7}, {"minecraft:leatherworker", 8},
				{"minecraft:librarian", 9}, {"minecraft:mason", 10}, {"minecraft:nitwit", 11}, {"minecraft:none", 0},
				{"minecraft:shepherd", 12}, {"minecraft:toolsmith", 13}, {"minecraft:weaponsmith", 14}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getVillagerType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:desert", 0}, {"minecraft:jungle", 1}, {"minecraft:plains", 2}, {"minecraft:savanna", 3},
				{"minecraft:snow", 4}, {"minecraft:swamp", 5}, {"minecraft:taiga", 6}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenBiomeSource() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:checkerboard", 2}, {"minecraft:fixed", 0}, {"minecraft:multi_noise", 1}, {"minecraft:the_end", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenBlockStateProviderType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:dual_noise_provider", 4}, {"minecraft:noise_provider", 3}, {"minecraft:noise_threshold_provider", 2}, {"minecraft:randomized_int_state_provider", 6},
				{"minecraft:rotated_block_provider", 5}, {"minecraft:simple_state_provider", 0}, {"minecraft:weighted_state_provider", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenCarver() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:canyon", 2}, {"minecraft:cave", 0}, {"minecraft:nether_cave", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenChunkGenerator() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:debug", 2}, {"minecraft:flat", 1}, {"minecraft:noise", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenDensityFunctionType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:abs", 19}, {"minecraft:add", 26}, {"minecraft:beardifier", 2}, {"minecraft:blend_alpha", 0},
				{"minecraft:blend_density", 17}, {"minecraft:blend_offset", 1}, {"minecraft:cache_2d", 6}, {"minecraft:cache_all_in_cell", 8},
				{"minecraft:cache_once", 7}, {"minecraft:clamp", 18}, {"minecraft:constant", 31}, {"minecraft:cube", 21},
				{"minecraft:end_islands", 10}, {"minecraft:find_top_surface", 33}, {"minecraft:flat_cache", 5}, {"minecraft:half_negative", 22},
				{"minecraft:interpolated", 4}, {"minecraft:invert", 24}, {"minecraft:max", 29}, {"minecraft:min", 28},
				{"minecraft:mul", 27}, {"minecraft:noise", 9}, {"minecraft:old_blended_noise", 3}, {"minecraft:quarter_negative", 23},
				{"minecraft:range_choice", 13}, {"minecraft:shift", 16}, {"minecraft:shift_a", 14}, {"minecraft:shift_b", 15},
				{"minecraft:shifted_noise", 12}, {"minecraft:spline", 30}, {"minecraft:square", 20}, {"minecraft:squeeze", 25},
				{"minecraft:weird_scaled_sampler", 11}, {"minecraft:y_clamped_gradient", 32}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenFeature() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:bamboo", 43}, {"minecraft:basalt_columns", 48}, {"minecraft:basalt_pillar", 53}, {"minecraft:block_column", 19},
				{"minecraft:block_pile", 6}, {"minecraft:blue_ice", 26}, {"minecraft:bonus_chest", 52}, {"minecraft:chorus_plant", 8},
				{"minecraft:coral_claw", 40}, {"minecraft:coral_mushroom", 39}, {"minecraft:coral_tree", 38}, {"minecraft:delta_feature", 49},
				{"minecraft:desert_well", 11}, {"minecraft:disk", 29}, {"minecraft:dripstone_cluster", 59}, {"minecraft:end_gateway", 35},
				{"minecraft:end_island", 34}, {"minecraft:end_platform", 32}, {"minecraft:end_spike", 33}, {"minecraft:fallen_tree", 2},
				{"minecraft:fill_layer", 51}, {"minecraft:flower", 3}, {"minecraft:forest_rock", 28}, {"minecraft:fossil", 12},
				{"minecraft:freeze_top_layer", 17}, {"minecraft:geode", 58}, {"minecraft:glowstone_blob", 16}, {"minecraft:huge_brown_mushroom", 14},
				{"minecraft:huge_fungus", 44}, {"minecraft:huge_red_mushroom", 13}, {"minecraft:ice_spike", 15}, {"minecraft:iceberg", 27},
				{"minecraft:kelp", 37}, {"minecraft:lake", 30}, {"minecraft:large_dripstone", 60}, {"minecraft:monster_room", 25},
				{"minecraft:multiface_growth", 23}, {"minecraft:nether_forest_vegetation", 45}, {"minecraft:netherrack_replace_blobs", 50}, {"minecraft:no_bonemeal_flower", 4},
				{"minecraft:no_op", 0}, {"minecraft:ore", 31}, {"minecraft:pointed_dripstone", 61}, {"minecraft:random_boolean_selector", 57},
				{"minecraft:random_patch", 5}, {"minecraft:random_selector", 55}, {"minecraft:replace_single_block", 9}, {"minecraft:root_system", 22},
				{"minecraft:scattered_ore", 54}, {"minecraft:sculk_patch", 62}, {"minecraft:sea_pickle", 41}, {"minecraft:seagrass", 36},
				{"minecraft:simple_block", 42}, {"minecraft:simple_random_selector", 56}, {"minecraft:spring_feature", 7}, {"minecraft:tree", 1},
				{"minecraft:twisting_vines", 47}, {"minecraft:underwater_magma", 24}, {"minecraft:vegetation_patch", 20}, {"minecraft:vines", 18},
				{"minecraft:void_start_platform", 10}, {"minecraft:waterlogged_vegetation_patch", 21}, {"minecraft:weeping_vines", 46}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenFeatureSizeType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:three_layers_feature_size", 1}, {"minecraft:two_layers_feature_size", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenFoliagePlacerType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:acacia_foliage_placer", 3}, {"minecraft:blob_foliage_placer", 0}, {"minecraft:bush_foliage_placer", 4}, {"minecraft:cherry_foliage_placer", 10},
				{"minecraft:dark_oak_foliage_placer", 8}, {"minecraft:fancy_foliage_placer", 5}, {"minecraft:jungle_foliage_placer", 6}, {"minecraft:mega_pine_foliage_placer", 7},
				{"minecraft:pine_foliage_placer", 2}, {"minecraft:random_spread_foliage_placer", 9}, {"minecraft:spruce_foliage_placer", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenMaterialCondition() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:above_preliminary_surface", 9}, {"minecraft:biome", 0}, {"minecraft:hole", 8}, {"minecraft:noise_threshold", 1},
				{"minecraft:not", 7}, {"minecraft:steep", 6}, {"minecraft:stone_depth", 10}, {"minecraft:temperature", 5},
				{"minecraft:vertical_gradient", 2}, {"minecraft:water", 4}, {"minecraft:y_above", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenMaterialRule() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:bandlands", 0}, {"minecraft:block", 1}, {"minecraft:condition", 3}, {"minecraft:sequence", 2}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenPlacementModifierType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:biome", 4}, {"minecraft:block_predicate_filter", 0}, {"minecraft:count", 5}, {"minecraft:count_on_every_layer", 8},
				{"minecraft:environment_scan", 9}, {"minecraft:fixed_placement", 14}, {"minecraft:height_range", 11}, {"minecraft:heightmap", 10},
				{"minecraft:in_square", 12}, {"minecraft:noise_based_count", 6}, {"minecraft:noise_threshold_count", 7}, {"minecraft:random_offset", 13},
				{"minecraft:rarity_filter", 1}, {"minecraft:surface_relative_threshold_filter", 2}, {"minecraft:surface_water_depth_filter", 3}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenPoolAliasBinding() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:direct", 2}, {"minecraft:random", 0}, {"minecraft:random_group", 1}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenRootPlacerType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:mangrove_root_placer", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenStructurePiece() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:btp", 52}, {"minecraft:ecp", 50}, {"minecraft:iglu", 34}, {"minecraft:jigsaw", 55},
				{"minecraft:mscorridor", 0}, {"minecraft:mscrossing", 1}, {"minecraft:msroom", 2}, {"minecraft:msstairs", 3},
				{"minecraft:nebcr", 4}, {"minecraft:nebef", 5}, {"minecraft:nebs", 6}, {"minecraft:neccs", 7},
				{"minecraft:nece", 9}, {"minecraft:necsr", 14}, {"minecraft:nectb", 8}, {"minecraft:nefos", 54},
				{"minecraft:nemt", 15}, {"minecraft:nerc", 16}, {"minecraft:nesc", 12}, {"minecraft:nesclt", 11},
				{"minecraft:nescrt", 13}, {"minecraft:nescsc", 10}, {"minecraft:nesr", 17}, {"minecraft:nestart", 18},
				{"minecraft:omb", 38}, {"minecraft:omcr", 39}, {"minecraft:omdxr", 40}, {"minecraft:omdxyr", 41},
				{"minecraft:omdyr", 42}, {"minecraft:omdyzr", 43}, {"minecraft:omdzr", 44}, {"minecraft:omentry", 45},
				{"minecraft:ompenthouse", 46}, {"minecraft:omsimple", 47}, {"minecraft:omsimplet", 48}, {"minecraft:omwr", 49},
				{"minecraft:orp", 33}, {"minecraft:rupo", 35}, {"minecraft:sh5c", 21}, {"minecraft:shcc", 19},
				{"minecraft:shfc", 20}, {"minecraft:shipwreck", 53}, {"minecraft:shli", 23}, {"minecraft:shlt", 22},
				{"minecraft:shph", 25}, {"minecraft:shpr", 24}, {"minecraft:shrc", 27}, {"minecraft:shrt", 26},
				{"minecraft:shs", 30}, {"minecraft:shsd", 28}, {"minecraft:shssd", 31}, {"minecraft:shstart", 29},
				{"minecraft:tedp", 37}, {"minecraft:tejp", 32}, {"minecraft:tesh", 36}, {"minecraft:wmp", 51}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenStructurePlacement() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:concentric_rings", 1}, {"minecraft:random_spread", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenStructurePoolElement() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:empty_pool_element", 3}, {"minecraft:feature_pool_element", 2}, {"minecraft:legacy_single_pool_element", 4}, {"minecraft:list_pool_element", 1},
				{"minecraft:single_pool_element", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenStructureProcessor() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:blackstone_replace", 7}, {"minecraft:block_age", 6}, {"minecraft:block_ignore", 0}, {"minecraft:block_rot", 1},
				{"minecraft:capped", 10}, {"minecraft:gravity", 2}, {"minecraft:jigsaw_replacement", 3}, {"minecraft:lava_submerged_block", 8},
				{"minecraft:nop", 5}, {"minecraft:protected_blocks", 9}, {"minecraft:rule", 4}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenStructureType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:buried_treasure", 0}, {"minecraft:desert_pyramid", 1}, {"minecraft:end_city", 2}, {"minecraft:fortress", 3},
				{"minecraft:igloo", 4}, {"minecraft:jigsaw", 5}, {"minecraft:jungle_temple", 6}, {"minecraft:mineshaft", 7},
				{"minecraft:nether_fossil", 8}, {"minecraft:ocean_monument", 9}, {"minecraft:ocean_ruin", 10}, {"minecraft:ruined_portal", 11},
				{"minecraft:shipwreck", 12}, {"minecraft:stronghold", 13}, {"minecraft:swamp_hut", 14}, {"minecraft:woodland_mansion", 15}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenTreeDecoratorType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:alter_ground", 6}, {"minecraft:attached_to_leaves", 7}, {"minecraft:attached_to_logs", 9}, {"minecraft:beehive", 5},
				{"minecraft:cocoa", 4}, {"minecraft:creaking_heart", 3}, {"minecraft:leave_vine", 1}, {"minecraft:pale_moss", 2},
				{"minecraft:place_on_ground", 8}, {"minecraft:trunk_vine", 0}
		};
		return registry;
	}

	static std::map<std::string, uint32_t> getWorldgenTrunkPlacerType() {
		static std::map<std::string, uint32_t> registry = {
				{"minecraft:bending_trunk_placer", 6}, {"minecraft:cherry_trunk_placer", 8}, {"minecraft:dark_oak_trunk_placer", 4}, {"minecraft:fancy_trunk_placer", 5},
				{"minecraft:forking_trunk_placer", 1}, {"minecraft:giant_trunk_placer", 2}, {"minecraft:mega_jungle_trunk_placer", 3}, {"minecraft:straight_trunk_placer", 0},
				{"minecraft:upwards_branching_trunk_placer", 7}
		};
		return registry;
	}

	uint32_t getRegistryId(const std::string& registry, const std::string& key) {
		static std::map<std::string, std::map<std::string, uint32_t>> allRegistries = {
				{"minecraft:activity", getActivity()},
				{"minecraft:attribute", getAttribute()},
				{"minecraft:block", getBlock()},
				{"minecraft:block_entity_type", getBlockEntityType()},
				{"minecraft:block_predicate_type", getBlockPredicateType()},
				{"minecraft:block_type", getBlockType()},
				{"minecraft:chunk_status", getChunkStatus()},
				{"minecraft:command_argument_type", getCommandArgumentType()},
				{"minecraft:consume_effect_type", getConsumeEffectType()},
				{"minecraft:creative_mode_tab", getCreativeModeTab()},
				{"minecraft:custom_stat", getCustomStat()},
				{"minecraft:data_component_predicate_type", getDataComponentPredicateType()},
				{"minecraft:data_component_type", getDataComponentType()},
				{"minecraft:debug_subscription", getDebugSubscription()},
				{"minecraft:decorated_pot_pattern", getDecoratedPotPattern()},
				{"minecraft:dialog_action_type", getDialogActionType()},
				{"minecraft:dialog_body_type", getDialogBodyType()},
				{"minecraft:dialog_type", getDialogType()},
				{"minecraft:enchantment_effect_component_type", getEnchantmentEffectComponentType()},
				{"minecraft:enchantment_entity_effect_type", getEnchantmentEntityEffectType()},
				{"minecraft:enchantment_level_based_value_type", getEnchantmentLevelBasedValueType()},
				{"minecraft:enchantment_location_based_effect_type", getEnchantmentLocationBasedEffectType()},
				{"minecraft:enchantment_provider_type", getEnchantmentProviderType()},
				{"minecraft:enchantment_value_effect_type", getEnchantmentValueEffectType()},
				{"minecraft:entity_sub_predicate_type", getEntitySubPredicateType()},
				{"minecraft:entity_type", getEntityType()},
				{"minecraft:float_provider_type", getFloatProviderType()},
				{"minecraft:fluid", getFluid()},
				{"minecraft:game_event", getGameEvent()},
				{"minecraft:height_provider_type", getHeightProviderType()},
				{"minecraft:incoming_rpc_methods", getIncomingRpcMethods()},
				{"minecraft:input_control_type", getInputControlType()},
				{"minecraft:int_provider_type", getIntProviderType()},
				{"minecraft:item", getItem()},
				{"minecraft:loot_condition_type", getLootConditionType()},
				{"minecraft:loot_function_type", getLootFunctionType()},
				{"minecraft:loot_nbt_provider_type", getLootNbtProviderType()},
				{"minecraft:loot_number_provider_type", getLootNumberProviderType()},
				{"minecraft:loot_pool_entry_type", getLootPoolEntryType()},
				{"minecraft:loot_score_provider_type", getLootScoreProviderType()},
				{"minecraft:map_decoration_type", getMapDecorationType()},
				{"minecraft:memory_module_type", getMemoryModuleType()},
				{"minecraft:menu", getMenu()},
				{"minecraft:mob_effect", getMobEffect()},
				{"minecraft:number_format_type", getNumberFormatType()},
				{"minecraft:outgoing_rpc_methods", getOutgoingRpcMethods()},
				{"minecraft:particle_type", getParticleType()},
				{"minecraft:point_of_interest_type", getPointOfInterestType()},
				{"minecraft:pos_rule_test", getPosRuleTest()},
				{"minecraft:position_source_type", getPositionSourceType()},
				{"minecraft:potion", getPotion()},
				{"minecraft:recipe_book_category", getRecipeBookCategory()},
				{"minecraft:recipe_display", getRecipeDisplay()},
				{"minecraft:recipe_serializer", getRecipeSerializer()},
				{"minecraft:recipe_type", getRecipeType()},
				{"minecraft:rule_block_entity_modifier", getRuleBlockEntityModifier()},
				{"minecraft:rule_test", getRuleTest()},
				{"minecraft:schedule", getSchedule()},
				{"minecraft:sensor_type", getSensorType()},
				{"minecraft:slot_display", getSlotDisplay()},
				{"minecraft:sound_event", getSoundEvent()},
				{"minecraft:spawn_condition_type", getSpawnConditionType()},
				{"minecraft:stat_type", getStatType()},
				{"minecraft:test_environment_definition_type", getTestEnvironmentDefinitionType()},
				{"minecraft:test_function", getTestFunction()},
				{"minecraft:test_instance_type", getTestInstanceType()},
				{"minecraft:ticket_type", getTicketType()},
				{"minecraft:trigger_type", getTriggerType()},
				{"minecraft:villager_profession", getVillagerProfession()},
				{"minecraft:villager_type", getVillagerType()},
				{"minecraft:worldgen/biome_source", getWorldgenBiomeSource()},
				{"minecraft:worldgen/block_state_provider_type", getWorldgenBlockStateProviderType()},
				{"minecraft:worldgen/carver", getWorldgenCarver()},
				{"minecraft:worldgen/chunk_generator", getWorldgenChunkGenerator()},
				{"minecraft:worldgen/density_function_type", getWorldgenDensityFunctionType()},
				{"minecraft:worldgen/feature", getWorldgenFeature()},
				{"minecraft:worldgen/feature_size_type", getWorldgenFeatureSizeType()},
				{"minecraft:worldgen/foliage_placer_type", getWorldgenFoliagePlacerType()},
				{"minecraft:worldgen/material_condition", getWorldgenMaterialCondition()},
				{"minecraft:worldgen/material_rule", getWorldgenMaterialRule()},
				{"minecraft:worldgen/placement_modifier_type", getWorldgenPlacementModifierType()},
				{"minecraft:worldgen/pool_alias_binding", getWorldgenPoolAliasBinding()},
				{"minecraft:worldgen/root_placer_type", getWorldgenRootPlacerType()},
				{"minecraft:worldgen/structure_piece", getWorldgenStructurePiece()},
				{"minecraft:worldgen/structure_placement", getWorldgenStructurePlacement()},
				{"minecraft:worldgen/structure_pool_element", getWorldgenStructurePoolElement()},
				{"minecraft:worldgen/structure_processor", getWorldgenStructureProcessor()},
				{"minecraft:worldgen/structure_type", getWorldgenStructureType()},
				{"minecraft:worldgen/tree_decorator_type", getWorldgenTreeDecoratorType()},
				{"minecraft:worldgen/trunk_placer_type", getWorldgenTrunkPlacerType()}
		};

		auto registryIt = allRegistries.find(registry);
		if (registryIt == allRegistries.end()) {
			return 0;
		}

		auto keyIt = registryIt->second.find(key);
		if (keyIt == registryIt->second.end()) {
			return 0;
		}

		return keyIt->second;
	}

	std::vector<std::string> getAvailableRegistries() {
		return {
				"minecraft:activity",
				"minecraft:attribute",
				"minecraft:block",
				"minecraft:block_entity_type",
				"minecraft:block_predicate_type",
				"minecraft:block_type",
				"minecraft:chunk_status",
				"minecraft:command_argument_type",
				"minecraft:consume_effect_type",
				"minecraft:creative_mode_tab",
				"minecraft:custom_stat",
				"minecraft:data_component_predicate_type",
				"minecraft:data_component_type",
				"minecraft:debug_subscription",
				"minecraft:decorated_pot_pattern",
				"minecraft:dialog_action_type",
				"minecraft:dialog_body_type",
				"minecraft:dialog_type",
				"minecraft:enchantment_effect_component_type",
				"minecraft:enchantment_entity_effect_type",
				"minecraft:enchantment_level_based_value_type",
				"minecraft:enchantment_location_based_effect_type",
				"minecraft:enchantment_provider_type",
				"minecraft:enchantment_value_effect_type",
				"minecraft:entity_sub_predicate_type",
				"minecraft:entity_type",
				"minecraft:float_provider_type",
				"minecraft:fluid",
				"minecraft:game_event",
				"minecraft:height_provider_type",
				"minecraft:incoming_rpc_methods",
				"minecraft:input_control_type",
				"minecraft:int_provider_type",
				"minecraft:item",
				"minecraft:loot_condition_type",
				"minecraft:loot_function_type",
				"minecraft:loot_nbt_provider_type",
				"minecraft:loot_number_provider_type",
				"minecraft:loot_pool_entry_type",
				"minecraft:loot_score_provider_type",
				"minecraft:map_decoration_type",
				"minecraft:memory_module_type",
				"minecraft:menu",
				"minecraft:mob_effect",
				"minecraft:number_format_type",
				"minecraft:outgoing_rpc_methods",
				"minecraft:particle_type",
				"minecraft:point_of_interest_type",
				"minecraft:pos_rule_test",
				"minecraft:position_source_type",
				"minecraft:potion",
				"minecraft:recipe_book_category",
				"minecraft:recipe_display",
				"minecraft:recipe_serializer",
				"minecraft:recipe_type",
				"minecraft:rule_block_entity_modifier",
				"minecraft:rule_test",
				"minecraft:schedule",
				"minecraft:sensor_type",
				"minecraft:slot_display",
				"minecraft:sound_event",
				"minecraft:spawn_condition_type",
				"minecraft:stat_type",
				"minecraft:test_environment_definition_type",
				"minecraft:test_function",
				"minecraft:test_instance_type",
				"minecraft:ticket_type",
				"minecraft:trigger_type",
				"minecraft:villager_profession",
				"minecraft:villager_type",
				"minecraft:worldgen/biome_source",
				"minecraft:worldgen/block_state_provider_type",
				"minecraft:worldgen/carver",
				"minecraft:worldgen/chunk_generator",
				"minecraft:worldgen/density_function_type",
				"minecraft:worldgen/feature",
				"minecraft:worldgen/feature_size_type",
				"minecraft:worldgen/foliage_placer_type",
				"minecraft:worldgen/material_condition",
				"minecraft:worldgen/material_rule",
				"minecraft:worldgen/placement_modifier_type",
				"minecraft:worldgen/pool_alias_binding",
				"minecraft:worldgen/root_placer_type",
				"minecraft:worldgen/structure_piece",
				"minecraft:worldgen/structure_placement",
				"minecraft:worldgen/structure_pool_element",
				"minecraft:worldgen/structure_processor",
				"minecraft:worldgen/structure_type",
				"minecraft:worldgen/tree_decorator_type",
				"minecraft:worldgen/trunk_placer_type"
		};
	}
};

#endif // REGISTRY_IDS_HPP
