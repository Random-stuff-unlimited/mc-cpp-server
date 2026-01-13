#pragma once
// Minecraft Registry Data
// Generated automatically - do not modify

#include <unordered_map>
#include <vector>
#include <string>

struct RegistryEntry {
    std::string name;
    int protocol_id;
};

struct Registry {
    std::vector<RegistryEntry> entries;
    int protocol_id;
};

// Complete list of registries in the exact order of Minecraft protocol 1.21.5
const std::vector<std::string> REGISTRY_LIST = {
    "minecraft:worldgen/biome",
    "minecraft:chat_type",
    "minecraft:trim_pattern",
    "minecraft:trim_material",
    "minecraft:wolf_variant",
    "minecraft:wolf_sound_variant",
    "minecraft:pig_variant",
    "minecraft:frog_variant",
    "minecraft:cat_variant",
    "minecraft:cow_variant",
    "minecraft:chicken_variant",
    "minecraft:dimension_type",
    "minecraft:banner_pattern",
    "minecraft:enchantment",
    "minecraft:jukebox_song",
    "minecraft:instrument",
    "minecraft:test_environment",
    "minecraft:test_instance",
    "minecraft:dialog",
    "minecraft:damage_type",
    "minecraft:painting_variant"
};

const std::unordered_map<std::string, Registry> REGISTRIES = {
    {"minecraft:banner_pattern", {
        {
            {"minecraft:base", 9},
            {"minecraft:border", 16},
            {"minecraft:bricks", 28},
            {"minecraft:circle", 21},
            {"minecraft:creeper", 17},
            {"minecraft:cross", 18},
            {"minecraft:curly_border", 23},
            {"minecraft:diagonal_left", 42},
            {"minecraft:diagonal_right", 25},
            {"minecraft:diagonal_up_left", 3},
            {"minecraft:diagonal_up_right", 5},
            {"minecraft:flow", 31},
            {"minecraft:flower", 41},
            {"minecraft:globe", 6},
            {"minecraft:gradient", 39},
            {"minecraft:gradient_up", 38},
            {"minecraft:guster", 0},
            {"minecraft:half_horizontal", 19},
            {"minecraft:half_horizontal_bottom", 11},
            {"minecraft:half_vertical", 40},
            {"minecraft:half_vertical_right", 37},
            {"minecraft:mojang", 12},
            {"minecraft:piglin", 4},
            {"minecraft:rhombus", 1},
            {"minecraft:skull", 27},
            {"minecraft:small_stripes", 8},
            {"minecraft:square_bottom_left", 14},
            {"minecraft:square_bottom_right", 36},
            {"minecraft:square_top_left", 34},
            {"minecraft:square_top_right", 32},
            {"minecraft:straight_cross", 13},
            {"minecraft:stripe_bottom", 15},
            {"minecraft:stripe_center", 20},
            {"minecraft:stripe_downleft", 2},
            {"minecraft:stripe_downright", 33},
            {"minecraft:stripe_left", 10},
            {"minecraft:stripe_middle", 29},
            {"minecraft:stripe_right", 22},
            {"minecraft:stripe_top", 26},
            {"minecraft:triangle_bottom", 7},
            {"minecraft:triangle_top", 30},
            {"minecraft:triangles_bottom", 24},
            {"minecraft:triangles_top", 35}
        },
        0
    }},
    {"minecraft:cat_variant", {
        {
            {"minecraft:all_black", 0},
            {"minecraft:black", 8},
            {"minecraft:british_shorthair", 5},
            {"minecraft:calico", 1},
            {"minecraft:jellie", 7},
            {"minecraft:persian", 6},
            {"minecraft:ragdoll", 4},
            {"minecraft:red", 3},
            {"minecraft:siamese", 2},
            {"minecraft:tabby", 9},
            {"minecraft:white", 10}
        },
        0
    }},
    {"minecraft:chat_type", {
        {
            {"minecraft:chat", 1},
            {"minecraft:emote_command", 2},
            {"minecraft:msg_command_incoming", 5},
            {"minecraft:msg_command_outgoing", 4},
            {"minecraft:say_command", 0},
            {"minecraft:team_msg_command_incoming", 3},
            {"minecraft:team_msg_command_outgoing", 6}
        },
        0
    }},
    {"minecraft:chicken_variant", {
        {
            {"minecraft:cold", 1},
            {"minecraft:temperate", 0},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:cow_variant", {
        {
            {"minecraft:cold", 1},
            {"minecraft:temperate", 0},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:damage_type", {
        {
            {"minecraft:arrow", 12},
            {"minecraft:bad_respawn_point", 17},
            {"minecraft:cactus", 18},
            {"minecraft:campfire", 3},
            {"minecraft:cramming", 4},
            {"minecraft:dragon_breath", 30},
            {"minecraft:drown", 29},
            {"minecraft:dry_out", 14},
            {"minecraft:ender_pearl", 27},
            {"minecraft:explosion", 13},
            {"minecraft:fall", 15},
            {"minecraft:falling_anvil", 24},
            {"minecraft:falling_block", 6},
            {"minecraft:falling_stalactite", 19},
            {"minecraft:fireball", 11},
            {"minecraft:fireworks", 2},
            {"minecraft:fly_into_wall", 20},
            {"minecraft:freeze", 36},
            {"minecraft:generic", 38},
            {"minecraft:generic_kill", 48},
            {"minecraft:hot_floor", 45},
            {"minecraft:in_fire", 34},
            {"minecraft:in_wall", 23},
            {"minecraft:indirect_magic", 42},
            {"minecraft:lava", 33},
            {"minecraft:lightning_bolt", 0},
            {"minecraft:mace_smash", 21},
            {"minecraft:magic", 9},
            {"minecraft:mob_attack", 32},
            {"minecraft:mob_attack_no_aggro", 7},
            {"minecraft:mob_projectile", 40},
            {"minecraft:on_fire", 44},
            {"minecraft:out_of_world", 8},
            {"minecraft:outside_border", 10},
            {"minecraft:player_attack", 47},
            {"minecraft:player_explosion", 25},
            {"minecraft:sonic_boom", 35},
            {"minecraft:spit", 37},
            {"minecraft:stalagmite", 1},
            {"minecraft:starve", 46},
            {"minecraft:sting", 31},
            {"minecraft:sweet_berry_bush", 5},
            {"minecraft:thorns", 41},
            {"minecraft:thrown", 43},
            {"minecraft:trident", 39},
            {"minecraft:unattributed_fireball", 16},
            {"minecraft:wind_charge", 28},
            {"minecraft:wither", 26},
            {"minecraft:wither_skull", 22}
        },
        0
    }},
    {"minecraft:dialog", {
        {
            {"minecraft:custom_options", 2},
            {"minecraft:quick_actions", 0},
            {"minecraft:server_links", 1}
        },
        0
    }},
    {"minecraft:dimension_type", {
        {
            {"minecraft:overworld", 3},
            {"minecraft:overworld_caves", 0},
            {"minecraft:the_end", 2},
            {"minecraft:the_nether", 1}
        },
        0
    }},
    {"minecraft:enchantment", {
        {
	        {"minecraft:aqua_affinity", 31},
	        {"minecraft:bane_of_arthropods", 10},
	        {"minecraft:binding_curse", 13},
	        {"minecraft:blast_protection", 34},
	        {"minecraft:breach", 24},
	        {"minecraft:channeling", 15},
	        {"minecraft:density", 20},
	        {"minecraft:depth_strider", 2},
	        {"minecraft:efficiency", 9},
	        {"minecraft:feather_falling", 29},
	        {"minecraft:fire_aspect", 22},
	        {"minecraft:fire_protection", 30},
	        {"minecraft:flame", 14},
	        {"minecraft:fortune", 7},
	        {"minecraft:frost_walker", 33},
	        {"minecraft:impaling", 19},
	        {"minecraft:infinity", 40},
	        {"minecraft:knockback", 39},
	        {"minecraft:looting", 16},
	        {"minecraft:loyalty", 25},
	        {"minecraft:luck_of_the_sea", 26},
	        {"minecraft:lure", 17},
	        {"minecraft:mending", 35},
	        {"minecraft:multishot", 37},
	        {"minecraft:piercing", 12},
	        {"minecraft:power", 5},
	        {"minecraft:projectile_protection", 32},
	        {"minecraft:protection", 4},
	        {"minecraft:punch", 38},
	        {"minecraft:quick_charge", 3},
	        {"minecraft:respiration", 11},
	        {"minecraft:riptide", 6},
	        {"minecraft:sharpness", 27},
	        {"minecraft:silk_touch", 41},
	        {"minecraft:smite", 1},
	        {"minecraft:soul_speed", 28},
	        {"minecraft:sweeping_edge", 8},
	        {"minecraft:swift_sneak", 18},
	        {"minecraft:thorns", 36},
	        {"minecraft:unbreaking", 0},
	        {"minecraft:vanishing_curse", 23},
	        {"minecraft:wind_burst", 21}
        },
        0
    }},
    {"minecraft:frog_variant", {
        {
            {"minecraft:cold", 1},
            {"minecraft:temperate", 0},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:instrument", {
        {
            {"minecraft:admire_goat_horn", 5},
            {"minecraft:call_goat_horn", 7},
            {"minecraft:dream_goat_horn", 1},
            {"minecraft:feel_goat_horn", 4},
            {"minecraft:ponder_goat_horn", 2},
            {"minecraft:seek_goat_horn", 6},
            {"minecraft:sing_goat_horn", 3},
            {"minecraft:yearn_goat_horn", 0}
        },
        0
    }},
    {"minecraft:jukebox_song", {
        {
            {"minecraft:11", 19},
            {"minecraft:13", 8},
            {"minecraft:5", 18},
            {"minecraft:blocks", 14},
            {"minecraft:cat", 11},
            {"minecraft:chirp", 9},
            {"minecraft:creator", 10},
            {"minecraft:creator_music_box", 0},
            {"minecraft:far", 20},
            {"minecraft:lava_chicken", 1},
            {"minecraft:mall", 2},
            {"minecraft:mellohi", 15},
            {"minecraft:otherside", 3},
            {"minecraft:pigstep", 17},
            {"minecraft:precipice", 16},
            {"minecraft:relic", 7},
            {"minecraft:stal", 13},
            {"minecraft:strad", 6},
            {"minecraft:tears", 4},
            {"minecraft:wait", 5},
            {"minecraft:ward", 12}
        },
        0
    }},
    {"minecraft:painting_variant", {
        {
            {"minecraft:alban", 45},
            {"minecraft:aztec", 49},
            {"minecraft:aztec2", 40},
            {"minecraft:backyard", 29},
            {"minecraft:baroque", 14},
            {"minecraft:bomb", 19},
            {"minecraft:bouquet", 20},
            {"minecraft:burning_skull", 38},
            {"minecraft:bust", 18},
            {"minecraft:cavebird", 50},
            {"minecraft:changing", 15},
            {"minecraft:cotan", 34},
            {"minecraft:courbet", 24},
            {"minecraft:creebet", 7},
            {"minecraft:dennis", 22},
            {"minecraft:donkey_kong", 27},
            {"minecraft:earth", 47},
            {"minecraft:endboss", 8},
            {"minecraft:fern", 2},
            {"minecraft:fighters", 6},
            {"minecraft:finding", 43},
            {"minecraft:fire", 17},
            {"minecraft:graham", 26},
            {"minecraft:humble", 12},
            {"minecraft:kebab", 36},
            {"minecraft:lowmist", 11},
            {"minecraft:match", 9},
            {"minecraft:meditative", 46},
            {"minecraft:orb", 28},
            {"minecraft:owlemons", 48},
            {"minecraft:passage", 33},
            {"minecraft:pigscene", 3},
            {"minecraft:plant", 39},
            {"minecraft:pointer", 31},
            {"minecraft:pond", 13},
            {"minecraft:pool", 35},
            {"minecraft:prairie_ride", 16},
            {"minecraft:sea", 32},
            {"minecraft:skeleton", 0},
            {"minecraft:skull_and_roses", 44},
            {"minecraft:stage", 25},
            {"minecraft:sunflowers", 41},
            {"minecraft:sunset", 5},
            {"minecraft:tides", 1},
            {"minecraft:unpacked", 10},
            {"minecraft:void", 37},
            {"minecraft:wanderer", 4},
            {"minecraft:wasteland", 21},
            {"minecraft:water", 42},
            {"minecraft:wind", 23},
            {"minecraft:wither", 30}
        },
        0
    }},
    {"minecraft:pig_variant", {
        {
            {"minecraft:cold", 1},
            {"minecraft:temperate", 0},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:test_environment", {
        {
            {"minecraft:default", 0}
        },
        0
    }},
    {"minecraft:test_instance", {
        {
            {"minecraft:always_pass", 0}
        },
        0
    }},
    {"minecraft:trim_material", {
        {
            {"minecraft:amethyst", 3},
            {"minecraft:copper", 4},
            {"minecraft:diamond", 2},
            {"minecraft:emerald", 5},
            {"minecraft:gold", 7},
            {"minecraft:iron", 10},
            {"minecraft:lapis", 6},
            {"minecraft:netherite", 1},
            {"minecraft:quartz", 0},
            {"minecraft:redstone", 9},
            {"minecraft:resin", 8}
        },
        0
    }},
    {"minecraft:trim_pattern", {
        {
            {"minecraft:bolt", 4},
            {"minecraft:coast", 0},
            {"minecraft:dune", 7},
            {"minecraft:eye", 10},
            {"minecraft:flow", 9},
            {"minecraft:host", 12},
            {"minecraft:raiser", 2},
            {"minecraft:rib", 17},
            {"minecraft:sentry", 6},
            {"minecraft:shaper", 1},
            {"minecraft:silence", 16},
            {"minecraft:snout", 15},
            {"minecraft:spire", 11},
            {"minecraft:tide", 14},
            {"minecraft:vex", 8},
            {"minecraft:ward", 13},
            {"minecraft:wayfinder", 5},
            {"minecraft:wild", 3}
        },
        0
    }},
    {"minecraft:wolf_sound_variant", {
        {
            {"minecraft:angry", 1},
            {"minecraft:big", 0},
            {"minecraft:classic", 4},
            {"minecraft:cute", 2},
            {"minecraft:grumpy", 5},
            {"minecraft:puglin", 6},
            {"minecraft:sad", 3}
        },
        0
    }},
    {"minecraft:wolf_variant", {
        {
            {"minecraft:ashen", 5},
            {"minecraft:black", 3},
            {"minecraft:chestnut", 2},
            {"minecraft:pale", 1},
            {"minecraft:rusty", 6},
            {"minecraft:snowy", 0},
            {"minecraft:spotted", 8},
            {"minecraft:striped", 4},
            {"minecraft:woods", 7}
        },
        0
    }},
    {"minecraft:worldgen/biome", {
        {
            {"minecraft:badlands", 30},
            {"minecraft:bamboo_jungle", 39},
            {"minecraft:basalt_deltas", 20},
            {"minecraft:beach", 15},
            {"minecraft:birch_forest", 9},
            {"minecraft:cherry_grove", 10},
            {"minecraft:cold_ocean", 32},
            {"minecraft:crimson_forest", 13},
            {"minecraft:dark_forest", 59},
            {"minecraft:deep_cold_ocean", 42},
            {"minecraft:deep_dark", 52},
            {"minecraft:deep_frozen_ocean", 2},
            {"minecraft:deep_lukewarm_ocean", 24},
            {"minecraft:deep_ocean", 31},
            {"minecraft:desert", 33},
            {"minecraft:dripstone_caves", 36},
            {"minecraft:end_barrens", 38},
            {"minecraft:end_highlands", 41},
            {"minecraft:end_midlands", 3},
            {"minecraft:eroded_badlands", 54},
            {"minecraft:flower_forest", 49},
            {"minecraft:forest", 40},
            {"minecraft:frozen_ocean", 6},
            {"minecraft:frozen_peaks", 25},
            {"minecraft:frozen_river", 63},
            {"minecraft:grove", 11},
            {"minecraft:ice_spikes", 56},
            {"minecraft:jagged_peaks", 27},
            {"minecraft:jungle", 1},
            {"minecraft:lukewarm_ocean", 61},
            {"minecraft:lush_caves", 46},
            {"minecraft:mangrove_swamp", 44},
            {"minecraft:meadow", 57},
            {"minecraft:mushroom_fields", 7},
            {"minecraft:nether_wastes", 28},
            {"minecraft:ocean", 26},
            {"minecraft:old_growth_birch_forest", 45},
            {"minecraft:old_growth_pine_taiga", 48},
            {"minecraft:old_growth_spruce_taiga", 4},
            {"minecraft:pale_garden", 19},
            {"minecraft:plains", 60},
            {"minecraft:river", 14},
            {"minecraft:savanna", 47},
            {"minecraft:savanna_plateau", 18},
            {"minecraft:small_end_islands", 21},
            {"minecraft:snowy_beach", 53},
            {"minecraft:snowy_plains", 43},
            {"minecraft:snowy_slopes", 16},
            {"minecraft:snowy_taiga", 50},
            {"minecraft:soul_sand_valley", 29},
            {"minecraft:sparse_jungle", 17},
            {"minecraft:stony_peaks", 62},
            {"minecraft:stony_shore", 35},
            {"minecraft:sunflower_plains", 22},
            {"minecraft:swamp", 5},
            {"minecraft:taiga", 51},
            {"minecraft:the_end", 37},
            {"minecraft:the_void", 34},
            {"minecraft:warm_ocean", 8},
            {"minecraft:warped_forest", 58},
            {"minecraft:windswept_forest", 12},
            {"minecraft:windswept_gravelly_hills", 23},
            {"minecraft:windswept_hills", 64},
            {"minecraft:windswept_savanna", 55},
            {"minecraft:wooded_badlands", 0}
        },
        0
    }}
};

// Helper functions for registries
inline const Registry* getRegistry(const std::string& name) {
    auto it = REGISTRIES.find(name);
    return (it != REGISTRIES.end()) ? &it->second : nullptr;
}

inline int getEntryProtocolId(const std::string& registry, const std::string& entry) {
    const Registry* reg = getRegistry(registry);
    if (!reg) return -1;

    for (const auto& e : reg->entries) {
        if (e.name == entry) return e.protocol_id;
    }
    return -1;
}

inline std::vector<std::string> getRegistryNames() {
    return REGISTRY_LIST;
}

inline size_t getRegistryCount() {
    return REGISTRY_LIST.size();
}

inline bool hasRegistry(const std::string& name) {
    return REGISTRIES.find(name) != REGISTRIES.end();
}

inline size_t getTotalEntryCount() {
    size_t count = 0;
    for (const auto& [name, registry] : REGISTRIES) {
        count += registry.entries.size();
    }
    return count;
}
