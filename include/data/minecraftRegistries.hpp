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

// Complete list of registries
const std::vector<std::string> REGISTRY_LIST = {
    "minecraft:banner_pattern",
    "minecraft:cat_variant",
    "minecraft:chat_type",
    "minecraft:chicken_variant",
    "minecraft:cow_variant",
    "minecraft:damage_type",
    "minecraft:dialog",
    "minecraft:dimension_type",
    "minecraft:enchantment",
    "minecraft:enchantment_provider",
    "minecraft:frog_variant",
    "minecraft:instrument",
    "minecraft:jukebox_song",
    "minecraft:painting_variant",
    "minecraft:pig_variant",
    "minecraft:test_environment",
    "minecraft:test_instance",
    "minecraft:trim_material",
    "minecraft:trim_pattern",
    "minecraft:wolf_sound_variant",
    "minecraft:wolf_variant",
    "minecraft:worldgen/biome"
};

const std::unordered_map<std::string, Registry> REGISTRIES = {
    {"minecraft:banner_pattern", {
        {
            {"minecraft:guster", 0},
            {"minecraft:rhombus", 1},
            {"minecraft:stripe_downleft", 2},
            {"minecraft:diagonal_up_left", 3},
            {"minecraft:piglin", 4},
            {"minecraft:diagonal_up_right", 5},
            {"minecraft:globe", 6},
            {"minecraft:triangle_bottom", 7},
            {"minecraft:small_stripes", 8},
            {"minecraft:base", 9},
            {"minecraft:stripe_left", 10},
            {"minecraft:half_horizontal_bottom", 11},
            {"minecraft:mojang", 12},
            {"minecraft:straight_cross", 13},
            {"minecraft:square_bottom_left", 14},
            {"minecraft:stripe_bottom", 15},
            {"minecraft:border", 16},
            {"minecraft:creeper", 17},
            {"minecraft:cross", 18},
            {"minecraft:half_horizontal", 19},
            {"minecraft:stripe_center", 20},
            {"minecraft:circle", 21},
            {"minecraft:stripe_right", 22},
            {"minecraft:curly_border", 23},
            {"minecraft:triangles_bottom", 24},
            {"minecraft:diagonal_right", 25},
            {"minecraft:stripe_top", 26},
            {"minecraft:skull", 27},
            {"minecraft:bricks", 28},
            {"minecraft:stripe_middle", 29},
            {"minecraft:triangle_top", 30},
            {"minecraft:flow", 31},
            {"minecraft:square_top_right", 32},
            {"minecraft:stripe_downright", 33},
            {"minecraft:square_top_left", 34},
            {"minecraft:triangles_top", 35},
            {"minecraft:square_bottom_right", 36},
            {"minecraft:half_vertical_right", 37},
            {"minecraft:gradient_up", 38},
            {"minecraft:gradient", 39},
            {"minecraft:half_vertical", 40},
            {"minecraft:flower", 41},
            {"minecraft:diagonal_left", 42}
        },
        0
    }},
    {"minecraft:cat_variant", {
        {
            {"minecraft:all_black", 0},
            {"minecraft:calico", 1},
            {"minecraft:siamese", 2},
            {"minecraft:red", 3},
            {"minecraft:ragdoll", 4},
            {"minecraft:british_shorthair", 5},
            {"minecraft:persian", 6},
            {"minecraft:jellie", 7},
            {"minecraft:black", 8},
            {"minecraft:tabby", 9},
            {"minecraft:white", 10}
        },
        0
    }},
    {"minecraft:chat_type", {
        {
            {"minecraft:say_command", 0},
            {"minecraft:chat", 1},
            {"minecraft:emote_command", 2},
            {"minecraft:team_msg_command_incoming", 3},
            {"minecraft:msg_command_outgoing", 4},
            {"minecraft:msg_command_incoming", 5},
            {"minecraft:team_msg_command_outgoing", 6}
        },
        0
    }},
    {"minecraft:chicken_variant", {
        {
            {"minecraft:temperate", 0},
            {"minecraft:cold", 1},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:cow_variant", {
        {
            {"minecraft:temperate", 0},
            {"minecraft:cold", 1},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:damage_type", {
        {
            {"minecraft:lightning_bolt", 0},
            {"minecraft:stalagmite", 1},
            {"minecraft:fireworks", 2},
            {"minecraft:campfire", 3},
            {"minecraft:cramming", 4},
            {"minecraft:sweet_berry_bush", 5},
            {"minecraft:falling_block", 6},
            {"minecraft:mob_attack_no_aggro", 7},
            {"minecraft:out_of_world", 8},
            {"minecraft:magic", 9},
            {"minecraft:outside_border", 10},
            {"minecraft:fireball", 11},
            {"minecraft:arrow", 12},
            {"minecraft:explosion", 13},
            {"minecraft:dry_out", 14},
            {"minecraft:fall", 15},
            {"minecraft:unattributed_fireball", 16},
            {"minecraft:bad_respawn_point", 17},
            {"minecraft:cactus", 18},
            {"minecraft:falling_stalactite", 19},
            {"minecraft:fly_into_wall", 20},
            {"minecraft:mace_smash", 21},
            {"minecraft:wither_skull", 22},
            {"minecraft:in_wall", 23},
            {"minecraft:falling_anvil", 24},
            {"minecraft:player_explosion", 25},
            {"minecraft:wither", 26},
            {"minecraft:ender_pearl", 27},
            {"minecraft:wind_charge", 28},
            {"minecraft:drown", 29},
            {"minecraft:dragon_breath", 30},
            {"minecraft:sting", 31},
            {"minecraft:mob_attack", 32},
            {"minecraft:lava", 33},
            {"minecraft:in_fire", 34},
            {"minecraft:sonic_boom", 35},
            {"minecraft:freeze", 36},
            {"minecraft:spit", 37},
            {"minecraft:generic", 38},
            {"minecraft:trident", 39},
            {"minecraft:mob_projectile", 40},
            {"minecraft:thorns", 41},
            {"minecraft:indirect_magic", 42},
            {"minecraft:thrown", 43},
            {"minecraft:on_fire", 44},
            {"minecraft:hot_floor", 45},
            {"minecraft:starve", 46},
            {"minecraft:player_attack", 47},
            {"minecraft:generic_kill", 48}
        },
        0
    }},
    {"minecraft:dialog", {
        {
            {"minecraft:quick_actions", 0},
            {"minecraft:server_links", 1},
            {"minecraft:custom_options", 2}
        },
        0
    }},
    {"minecraft:dimension_type", {
        {
            {"minecraft:overworld_caves", 0},
            {"minecraft:the_nether", 1},
            {"minecraft:the_end", 2},
            {"minecraft:overworld", 3}
        },
        0
    }},
    {"minecraft:enchantment", {
        {
            {"minecraft:unbreaking", 0},
            {"minecraft:smite", 1},
            {"minecraft:depth_strider", 2},
            {"minecraft:quick_charge", 3},
            {"minecraft:protection", 4},
            {"minecraft:power", 5},
            {"minecraft:riptide", 6},
            {"minecraft:fortune", 7},
            {"minecraft:sweeping_edge", 8},
            {"minecraft:efficiency", 9},
            {"minecraft:bane_of_arthropods", 10},
            {"minecraft:respiration", 11},
            {"minecraft:piercing", 12},
            {"minecraft:binding_curse", 13},
            {"minecraft:flame", 14},
            {"minecraft:channeling", 15},
            {"minecraft:looting", 16},
            {"minecraft:lure", 17},
            {"minecraft:swift_sneak", 18},
            {"minecraft:impaling", 19},
            {"minecraft:density", 20},
            {"minecraft:wind_burst", 21},
            {"minecraft:fire_aspect", 22},
            {"minecraft:vanishing_curse", 23},
            {"minecraft:breach", 24},
            {"minecraft:loyalty", 25},
            {"minecraft:luck_of_the_sea", 26},
            {"minecraft:sharpness", 27},
            {"minecraft:soul_speed", 28},
            {"minecraft:feather_falling", 29},
            {"minecraft:fire_protection", 30},
            {"minecraft:aqua_affinity", 31},
            {"minecraft:projectile_protection", 32},
            {"minecraft:frost_walker", 33},
            {"minecraft:blast_protection", 34},
            {"minecraft:mending", 35},
            {"minecraft:thorns", 36},
            {"minecraft:multishot", 37},
            {"minecraft:punch", 38},
            {"minecraft:knockback", 39},
            {"minecraft:infinity", 40},
            {"minecraft:silk_touch", 41}
        },
        0
    }},
    {"minecraft:enchantment_provider", {
        {
            {"minecraft:enderman_loot_drop", 0},
            {"minecraft:mob_spawn_equipment", 1},
            {"minecraft:pillager_spawn_crossbow", 2}
        },
        0
    }},
    {"minecraft:frog_variant", {
        {
            {"minecraft:temperate", 0},
            {"minecraft:cold", 1},
            {"minecraft:warm", 2}
        },
        0
    }},
    {"minecraft:instrument", {
        {
            {"minecraft:yearn_goat_horn", 0},
            {"minecraft:dream_goat_horn", 1},
            {"minecraft:ponder_goat_horn", 2},
            {"minecraft:sing_goat_horn", 3},
            {"minecraft:feel_goat_horn", 4},
            {"minecraft:admire_goat_horn", 5},
            {"minecraft:seek_goat_horn", 6},
            {"minecraft:call_goat_horn", 7}
        },
        0
    }},
    {"minecraft:jukebox_song", {
        {
            {"minecraft:creator_music_box", 0},
            {"minecraft:lava_chicken", 1},
            {"minecraft:mall", 2},
            {"minecraft:otherside", 3},
            {"minecraft:tears", 4},
            {"minecraft:wait", 5},
            {"minecraft:strad", 6},
            {"minecraft:relic", 7},
            {"minecraft:13", 8},
            {"minecraft:chirp", 9},
            {"minecraft:creator", 10},
            {"minecraft:cat", 11},
            {"minecraft:ward", 12},
            {"minecraft:stal", 13},
            {"minecraft:blocks", 14},
            {"minecraft:mellohi", 15},
            {"minecraft:precipice", 16},
            {"minecraft:pigstep", 17},
            {"minecraft:5", 18},
            {"minecraft:11", 19},
            {"minecraft:far", 20}
        },
        0
    }},
    {"minecraft:painting_variant", {
        {
            {"minecraft:skeleton", 0},
            {"minecraft:tides", 1},
            {"minecraft:fern", 2},
            {"minecraft:pigscene", 3},
            {"minecraft:wanderer", 4},
            {"minecraft:sunset", 5},
            {"minecraft:fighters", 6},
            {"minecraft:creebet", 7},
            {"minecraft:endboss", 8},
            {"minecraft:match", 9},
            {"minecraft:unpacked", 10},
            {"minecraft:lowmist", 11},
            {"minecraft:humble", 12},
            {"minecraft:pond", 13},
            {"minecraft:baroque", 14},
            {"minecraft:changing", 15},
            {"minecraft:prairie_ride", 16},
            {"minecraft:fire", 17},
            {"minecraft:bust", 18},
            {"minecraft:bomb", 19},
            {"minecraft:bouquet", 20},
            {"minecraft:wasteland", 21},
            {"minecraft:dennis", 22},
            {"minecraft:wind", 23},
            {"minecraft:courbet", 24},
            {"minecraft:stage", 25},
            {"minecraft:graham", 26},
            {"minecraft:donkey_kong", 27},
            {"minecraft:orb", 28},
            {"minecraft:backyard", 29},
            {"minecraft:wither", 30},
            {"minecraft:pointer", 31},
            {"minecraft:sea", 32},
            {"minecraft:passage", 33},
            {"minecraft:cotan", 34},
            {"minecraft:pool", 35},
            {"minecraft:kebab", 36},
            {"minecraft:void", 37},
            {"minecraft:burning_skull", 38},
            {"minecraft:plant", 39},
            {"minecraft:aztec2", 40},
            {"minecraft:sunflowers", 41},
            {"minecraft:water", 42},
            {"minecraft:finding", 43},
            {"minecraft:skull_and_roses", 44},
            {"minecraft:alban", 45},
            {"minecraft:meditative", 46},
            {"minecraft:earth", 47},
            {"minecraft:owlemons", 48},
            {"minecraft:aztec", 49},
            {"minecraft:cavebird", 50}
        },
        0
    }},
    {"minecraft:pig_variant", {
        {
            {"minecraft:temperate", 0},
            {"minecraft:cold", 1},
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
            {"minecraft:quartz", 0},
            {"minecraft:netherite", 1},
            {"minecraft:diamond", 2},
            {"minecraft:amethyst", 3},
            {"minecraft:copper", 4},
            {"minecraft:emerald", 5},
            {"minecraft:lapis", 6},
            {"minecraft:gold", 7},
            {"minecraft:resin", 8},
            {"minecraft:redstone", 9},
            {"minecraft:iron", 10}
        },
        0
    }},
    {"minecraft:trim_pattern", {
        {
            {"minecraft:coast", 0},
            {"minecraft:shaper", 1},
            {"minecraft:raiser", 2},
            {"minecraft:wild", 3},
            {"minecraft:bolt", 4},
            {"minecraft:wayfinder", 5},
            {"minecraft:sentry", 6},
            {"minecraft:dune", 7},
            {"minecraft:vex", 8},
            {"minecraft:flow", 9},
            {"minecraft:eye", 10},
            {"minecraft:spire", 11},
            {"minecraft:host", 12},
            {"minecraft:ward", 13},
            {"minecraft:tide", 14},
            {"minecraft:snout", 15},
            {"minecraft:silence", 16},
            {"minecraft:rib", 17}
        },
        0
    }},
    {"minecraft:wolf_sound_variant", {
        {
            {"minecraft:big", 0},
            {"minecraft:angry", 1},
            {"minecraft:cute", 2},
            {"minecraft:sad", 3},
            {"minecraft:classic", 4},
            {"minecraft:grumpy", 5},
            {"minecraft:puglin", 6}
        },
        0
    }},
    {"minecraft:wolf_variant", {
        {
            {"minecraft:snowy", 0},
            {"minecraft:pale", 1},
            {"minecraft:chestnut", 2},
            {"minecraft:black", 3},
            {"minecraft:striped", 4},
            {"minecraft:ashen", 5},
            {"minecraft:rusty", 6},
            {"minecraft:woods", 7},
            {"minecraft:spotted", 8}
        },
        0
    }},
    {"minecraft:worldgen/biome", {
        {
            {"minecraft:wooded_badlands", 0},
            {"minecraft:jungle", 1},
            {"minecraft:deep_frozen_ocean", 2},
            {"minecraft:end_midlands", 3},
            {"minecraft:old_growth_spruce_taiga", 4},
            {"minecraft:swamp", 5},
            {"minecraft:frozen_ocean", 6},
            {"minecraft:mushroom_fields", 7},
            {"minecraft:warm_ocean", 8},
            {"minecraft:birch_forest", 9},
            {"minecraft:cherry_grove", 10},
            {"minecraft:grove", 11},
            {"minecraft:windswept_forest", 12},
            {"minecraft:crimson_forest", 13},
            {"minecraft:river", 14},
            {"minecraft:beach", 15},
            {"minecraft:snowy_slopes", 16},
            {"minecraft:sparse_jungle", 17},
            {"minecraft:savanna_plateau", 18},
            {"minecraft:pale_garden", 19},
            {"minecraft:basalt_deltas", 20},
            {"minecraft:small_end_islands", 21},
            {"minecraft:sunflower_plains", 22},
            {"minecraft:windswept_gravelly_hills", 23},
            {"minecraft:deep_lukewarm_ocean", 24},
            {"minecraft:frozen_peaks", 25},
            {"minecraft:ocean", 26},
            {"minecraft:jagged_peaks", 27},
            {"minecraft:nether_wastes", 28},
            {"minecraft:soul_sand_valley", 29},
            {"minecraft:badlands", 30},
            {"minecraft:deep_ocean", 31},
            {"minecraft:cold_ocean", 32},
            {"minecraft:desert", 33},
            {"minecraft:the_void", 34},
            {"minecraft:stony_shore", 35},
            {"minecraft:dripstone_caves", 36},
            {"minecraft:the_end", 37},
            {"minecraft:end_barrens", 38},
            {"minecraft:bamboo_jungle", 39},
            {"minecraft:forest", 40},
            {"minecraft:end_highlands", 41},
            {"minecraft:deep_cold_ocean", 42},
            {"minecraft:snowy_plains", 43},
            {"minecraft:mangrove_swamp", 44},
            {"minecraft:old_growth_birch_forest", 45},
            {"minecraft:lush_caves", 46},
            {"minecraft:savanna", 47},
            {"minecraft:old_growth_pine_taiga", 48},
            {"minecraft:flower_forest", 49},
            {"minecraft:snowy_taiga", 50},
            {"minecraft:taiga", 51},
            {"minecraft:deep_dark", 52},
            {"minecraft:snowy_beach", 53},
            {"minecraft:eroded_badlands", 54},
            {"minecraft:windswept_savanna", 55},
            {"minecraft:ice_spikes", 56},
            {"minecraft:meadow", 57},
            {"minecraft:warped_forest", 58},
            {"minecraft:dark_forest", 59},
            {"minecraft:plains", 60},
            {"minecraft:lukewarm_ocean", 61},
            {"minecraft:stony_peaks", 62},
            {"minecraft:frozen_river", 63},
            {"minecraft:windswept_hills", 64}
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
