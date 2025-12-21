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
            {"minecraft:gradient", 0},
            {"minecraft:square_top_left", 1},
            {"minecraft:half_horizontal_bottom", 2},
            {"minecraft:stripe_bottom", 3},
            {"minecraft:flower", 4},
            {"minecraft:base", 5},
            {"minecraft:half_horizontal", 6},
            {"minecraft:stripe_middle", 7},
            {"minecraft:diagonal_up_left", 8},
            {"minecraft:triangles_top", 9},
            {"minecraft:stripe_right", 10},
            {"minecraft:square_bottom_left", 11},
            {"minecraft:border", 12},
            {"minecraft:stripe_top", 13},
            {"minecraft:triangles_bottom", 14},
            {"minecraft:skull", 15},
            {"minecraft:triangle_top", 16},
            {"minecraft:small_stripes", 17},
            {"minecraft:rhombus", 18},
            {"minecraft:half_vertical", 19},
            {"minecraft:square_top_right", 20},
            {"minecraft:guster", 21},
            {"minecraft:creeper", 22},
            {"minecraft:square_bottom_right", 23},
            {"minecraft:flow", 24},
            {"minecraft:cross", 25},
            {"minecraft:curly_border", 26},
            {"minecraft:straight_cross", 27},
            {"minecraft:bricks", 28},
            {"minecraft:diagonal_left", 29},
            {"minecraft:piglin", 30},
            {"minecraft:half_vertical_right", 31},
            {"minecraft:stripe_left", 32},
            {"minecraft:diagonal_up_right", 33},
            {"minecraft:stripe_downright", 34},
            {"minecraft:triangle_bottom", 35},
            {"minecraft:diagonal_right", 36},
            {"minecraft:stripe_downleft", 37},
            {"minecraft:circle", 38},
            {"minecraft:mojang", 39},
            {"minecraft:gradient_up", 40},
            {"minecraft:globe", 41},
            {"minecraft:stripe_center", 42}
        },
        0
    }},
    {"minecraft:cat_variant", {
        {
            {"minecraft:tabby", 0},
            {"minecraft:black", 1},
            {"minecraft:white", 2},
            {"minecraft:red", 3},
            {"minecraft:persian", 4},
            {"minecraft:british_shorthair", 5},
            {"minecraft:jellie", 6},
            {"minecraft:siamese", 7},
            {"minecraft:calico", 8},
            {"minecraft:ragdoll", 9},
            {"minecraft:all_black", 10}
        },
        0
    }},
    {"minecraft:chat_type", {
        {
            {"minecraft:chat", 0},
            {"minecraft:team_msg_command_outgoing", 1},
            {"minecraft:msg_command_incoming", 2},
            {"minecraft:say_command", 3},
            {"minecraft:emote_command", 4},
            {"minecraft:msg_command_outgoing", 5},
            {"minecraft:team_msg_command_incoming", 6}
        },
        0
    }},
    {"minecraft:chicken_variant", {
        {
            {"minecraft:cold", 0},
            {"minecraft:warm", 1},
            {"minecraft:temperate", 2}
        },
        0
    }},
    {"minecraft:cow_variant", {
        {
            {"minecraft:cold", 0},
            {"minecraft:warm", 1},
            {"minecraft:temperate", 2}
        },
        0
    }},
    {"minecraft:damage_type", {
        {
            {"minecraft:trident", 0},
            {"minecraft:drown", 1},
            {"minecraft:falling_stalactite", 2},
            {"minecraft:spit", 3},
            {"minecraft:player_explosion", 4},
            {"minecraft:lightning_bolt", 5},
            {"minecraft:mace_smash", 6},
            {"minecraft:ender_pearl", 7},
            {"minecraft:fly_into_wall", 8},
            {"minecraft:thorns", 9},
            {"minecraft:on_fire", 10},
            {"minecraft:stalagmite", 11},
            {"minecraft:in_fire", 12},
            {"minecraft:generic_kill", 13},
            {"minecraft:generic", 14},
            {"minecraft:thrown", 15},
            {"minecraft:unattributed_fireball", 16},
            {"minecraft:cramming", 17},
            {"minecraft:fireball", 18},
            {"minecraft:dragon_breath", 19},
            {"minecraft:explosion", 20},
            {"minecraft:sweet_berry_bush", 21},
            {"minecraft:fall", 22},
            {"minecraft:in_wall", 23},
            {"minecraft:hot_floor", 24},
            {"minecraft:wind_charge", 25},
            {"minecraft:mob_attack", 26},
            {"minecraft:wither", 27},
            {"minecraft:arrow", 28},
            {"minecraft:outside_border", 29},
            {"minecraft:player_attack", 30},
            {"minecraft:wither_skull", 31},
            {"minecraft:campfire", 32},
            {"minecraft:indirect_magic", 33},
            {"minecraft:sonic_boom", 34},
            {"minecraft:lava", 35},
            {"minecraft:starve", 36},
            {"minecraft:mob_attack_no_aggro", 37},
            {"minecraft:mob_projectile", 38},
            {"minecraft:freeze", 39},
            {"minecraft:dry_out", 40},
            {"minecraft:bad_respawn_point", 41},
            {"minecraft:cactus", 42},
            {"minecraft:fireworks", 43},
            {"minecraft:sting", 44},
            {"minecraft:magic", 45},
            {"minecraft:out_of_world", 46},
            {"minecraft:falling_block", 47},
            {"minecraft:falling_anvil", 48}
        },
        0
    }},
    {"minecraft:dialog", {
        {
            {"minecraft:custom_options", 0},
            {"minecraft:quick_actions", 1},
            {"minecraft:server_links", 2}
        },
        0
    }},
    {"minecraft:dimension_type", {
        {
            {"minecraft:the_end", 0},
            {"minecraft:the_nether", 1},
            {"minecraft:overworld", 2},
            {"minecraft:overworld_caves", 3}
        },
        0
    }},
    {"minecraft:enchantment", {
        {
            {"minecraft:luck_of_the_sea", 0},
            {"minecraft:fire_aspect", 1},
            {"minecraft:piercing", 2},
            {"minecraft:mending", 3},
            {"minecraft:breach", 4},
            {"minecraft:blast_protection", 5},
            {"minecraft:respiration", 6},
            {"minecraft:impaling", 7},
            {"minecraft:thorns", 8},
            {"minecraft:sharpness", 9},
            {"minecraft:soul_speed", 10},
            {"minecraft:unbreaking", 11},
            {"minecraft:silk_touch", 12},
            {"minecraft:bane_of_arthropods", 13},
            {"minecraft:wind_burst", 14},
            {"minecraft:sweeping_edge", 15},
            {"minecraft:loyalty", 16},
            {"minecraft:infinity", 17},
            {"minecraft:channeling", 18},
            {"minecraft:density", 19},
            {"minecraft:looting", 20},
            {"minecraft:lure", 21},
            {"minecraft:swift_sneak", 22},
            {"minecraft:binding_curse", 23},
            {"minecraft:depth_strider", 24},
            {"minecraft:fortune", 25},
            {"minecraft:flame", 26},
            {"minecraft:aqua_affinity", 27},
            {"minecraft:smite", 28},
            {"minecraft:efficiency", 29},
            {"minecraft:protection", 30},
            {"minecraft:feather_falling", 31},
            {"minecraft:frost_walker", 32},
            {"minecraft:vanishing_curse", 33},
            {"minecraft:riptide", 34},
            {"minecraft:punch", 35},
            {"minecraft:knockback", 36},
            {"minecraft:projectile_protection", 37},
            {"minecraft:quick_charge", 38},
            {"minecraft:multishot", 39},
            {"minecraft:fire_protection", 40},
            {"minecraft:power", 41}
        },
        0
    }},
    {"minecraft:enchantment_provider", {
        {
            {"minecraft:pillager_spawn_crossbow", 0},
            {"minecraft:enderman_loot_drop", 1},
            {"minecraft:mob_spawn_equipment", 2}
        },
        0
    }},
    {"minecraft:frog_variant", {
        {
            {"minecraft:cold", 0},
            {"minecraft:warm", 1},
            {"minecraft:temperate", 2}
        },
        0
    }},
    {"minecraft:instrument", {
        {
            {"minecraft:ponder_goat_horn", 0},
            {"minecraft:call_goat_horn", 1},
            {"minecraft:sing_goat_horn", 2},
            {"minecraft:yearn_goat_horn", 3},
            {"minecraft:feel_goat_horn", 4},
            {"minecraft:dream_goat_horn", 5},
            {"minecraft:admire_goat_horn", 6},
            {"minecraft:seek_goat_horn", 7}
        },
        0
    }},
    {"minecraft:jukebox_song", {
        {
            {"minecraft:otherside", 0},
            {"minecraft:11", 1},
            {"minecraft:relic", 2},
            {"minecraft:stal", 3},
            {"minecraft:far", 4},
            {"minecraft:pigstep", 5},
            {"minecraft:5", 6},
            {"minecraft:strad", 7},
            {"minecraft:mall", 8},
            {"minecraft:mellohi", 9},
            {"minecraft:creator", 10},
            {"minecraft:blocks", 11},
            {"minecraft:13", 12},
            {"minecraft:wait", 13},
            {"minecraft:chirp", 14},
            {"minecraft:lava_chicken", 15},
            {"minecraft:ward", 16},
            {"minecraft:tears", 17},
            {"minecraft:precipice", 18},
            {"minecraft:creator_music_box", 19},
            {"minecraft:cat", 20}
        },
        0
    }},
    {"minecraft:painting_variant", {
        {
            {"minecraft:passage", 0},
            {"minecraft:endboss", 1},
            {"minecraft:tides", 2},
            {"minecraft:owlemons", 3},
            {"minecraft:creebet", 4},
            {"minecraft:stage", 5},
            {"minecraft:pointer", 6},
            {"minecraft:void", 7},
            {"minecraft:backyard", 8},
            {"minecraft:water", 9},
            {"minecraft:orb", 10},
            {"minecraft:wasteland", 11},
            {"minecraft:aztec", 12},
            {"minecraft:donkey_kong", 13},
            {"minecraft:pool", 14},
            {"minecraft:fire", 15},
            {"minecraft:humble", 16},
            {"minecraft:dennis", 17},
            {"minecraft:burning_skull", 18},
            {"minecraft:cotan", 19},
            {"minecraft:match", 20},
            {"minecraft:pigscene", 21},
            {"minecraft:kebab", 22},
            {"minecraft:meditative", 23},
            {"minecraft:unpacked", 24},
            {"minecraft:lowmist", 25},
            {"minecraft:alban", 26},
            {"minecraft:fern", 27},
            {"minecraft:courbet", 28},
            {"minecraft:cavebird", 29},
            {"minecraft:sunflowers", 30},
            {"minecraft:wind", 31},
            {"minecraft:baroque", 32},
            {"minecraft:bomb", 33},
            {"minecraft:wither", 34},
            {"minecraft:bust", 35},
            {"minecraft:skeleton", 36},
            {"minecraft:sunset", 37},
            {"minecraft:sea", 38},
            {"minecraft:wanderer", 39},
            {"minecraft:aztec2", 40},
            {"minecraft:pond", 41},
            {"minecraft:changing", 42},
            {"minecraft:earth", 43},
            {"minecraft:finding", 44},
            {"minecraft:fighters", 45},
            {"minecraft:graham", 46},
            {"minecraft:skull_and_roses", 47},
            {"minecraft:bouquet", 48},
            {"minecraft:prairie_ride", 49},
            {"minecraft:plant", 50}
        },
        0
    }},
    {"minecraft:pig_variant", {
        {
            {"minecraft:cold", 0},
            {"minecraft:warm", 1},
            {"minecraft:temperate", 2}
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
            {"minecraft:lapis", 0},
            {"minecraft:copper", 1},
            {"minecraft:diamond", 2},
            {"minecraft:iron", 3},
            {"minecraft:netherite", 4},
            {"minecraft:quartz", 5},
            {"minecraft:emerald", 6},
            {"minecraft:amethyst", 7},
            {"minecraft:resin", 8},
            {"minecraft:gold", 9},
            {"minecraft:redstone", 10}
        },
        0
    }},
    {"minecraft:trim_pattern", {
        {
            {"minecraft:shaper", 0},
            {"minecraft:sentry", 1},
            {"minecraft:host", 2},
            {"minecraft:dune", 3},
            {"minecraft:coast", 4},
            {"minecraft:raiser", 5},
            {"minecraft:wayfinder", 6},
            {"minecraft:bolt", 7},
            {"minecraft:flow", 8},
            {"minecraft:spire", 9},
            {"minecraft:ward", 10},
            {"minecraft:eye", 11},
            {"minecraft:wild", 12},
            {"minecraft:tide", 13},
            {"minecraft:vex", 14},
            {"minecraft:rib", 15},
            {"minecraft:snout", 16},
            {"minecraft:silence", 17}
        },
        0
    }},
    {"minecraft:wolf_sound_variant", {
        {
            {"minecraft:angry", 0},
            {"minecraft:sad", 1},
            {"minecraft:big", 2},
            {"minecraft:cute", 3},
            {"minecraft:puglin", 4},
            {"minecraft:classic", 5},
            {"minecraft:grumpy", 6}
        },
        0
    }},
    {"minecraft:wolf_variant", {
        {
            {"minecraft:black", 0},
            {"minecraft:spotted", 1},
            {"minecraft:woods", 2},
            {"minecraft:snowy", 3},
            {"minecraft:rusty", 4},
            {"minecraft:ashen", 5},
            {"minecraft:chestnut", 6},
            {"minecraft:striped", 7},
            {"minecraft:pale", 8}
        },
        0
    }},
    {"minecraft:worldgen/biome", {
        {
            {"minecraft:deep_ocean", 0},
            {"minecraft:badlands", 1},
            {"minecraft:cherry_grove", 2},
            {"minecraft:dark_forest", 3},
            {"minecraft:the_end", 4},
            {"minecraft:frozen_peaks", 5},
            {"minecraft:old_growth_birch_forest", 6},
            {"minecraft:crimson_forest", 7},
            {"minecraft:mushroom_fields", 8},
            {"minecraft:birch_forest", 9},
            {"minecraft:end_barrens", 10},
            {"minecraft:frozen_ocean", 11},
            {"minecraft:mangrove_swamp", 12},
            {"minecraft:end_highlands", 13},
            {"minecraft:eroded_badlands", 14},
            {"minecraft:end_midlands", 15},
            {"minecraft:deep_dark", 16},
            {"minecraft:forest", 17},
            {"minecraft:meadow", 18},
            {"minecraft:lukewarm_ocean", 19},
            {"minecraft:warped_forest", 20},
            {"minecraft:deep_lukewarm_ocean", 21},
            {"minecraft:snowy_taiga", 22},
            {"minecraft:jagged_peaks", 23},
            {"minecraft:beach", 24},
            {"minecraft:lush_caves", 25},
            {"minecraft:old_growth_pine_taiga", 26},
            {"minecraft:flower_forest", 27},
            {"minecraft:windswept_forest", 28},
            {"minecraft:nether_wastes", 29},
            {"minecraft:basalt_deltas", 30},
            {"minecraft:pale_garden", 31},
            {"minecraft:small_end_islands", 32},
            {"minecraft:frozen_river", 33},
            {"minecraft:savanna_plateau", 34},
            {"minecraft:snowy_slopes", 35},
            {"minecraft:sunflower_plains", 36},
            {"minecraft:desert", 37},
            {"minecraft:snowy_beach", 38},
            {"minecraft:jungle", 39},
            {"minecraft:old_growth_spruce_taiga", 40},
            {"minecraft:stony_peaks", 41},
            {"minecraft:deep_frozen_ocean", 42},
            {"minecraft:soul_sand_valley", 43},
            {"minecraft:the_void", 44},
            {"minecraft:savanna", 45},
            {"minecraft:ocean", 46},
            {"minecraft:bamboo_jungle", 47},
            {"minecraft:snowy_plains", 48},
            {"minecraft:windswept_gravelly_hills", 49},
            {"minecraft:windswept_hills", 50},
            {"minecraft:river", 51},
            {"minecraft:sparse_jungle", 52},
            {"minecraft:deep_cold_ocean", 53},
            {"minecraft:wooded_badlands", 54},
            {"minecraft:grove", 55},
            {"minecraft:stony_shore", 56},
            {"minecraft:windswept_savanna", 57},
            {"minecraft:plains", 58},
            {"minecraft:cold_ocean", 59},
            {"minecraft:dripstone_caves", 60},
            {"minecraft:warm_ocean", 61},
            {"minecraft:taiga", 62},
            {"minecraft:swamp", 63},
            {"minecraft:ice_spikes", 64}
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
