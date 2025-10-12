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
    "minecraft:dimension_type",
    "minecraft:enchantment",
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
    "minecraft:wolf_variant"
};

const std::unordered_map<std::string, Registry> REGISTRIES = {
    {"minecraft:chat_type", {
        {
            {"emote_command", 0},
            {"say_command", 1},
            {"msg_command_incoming", 2},
            {"msg_command_outgoing", 3},
            {"team_msg_command_incoming", 4},
            {"chat", 5},
            {"team_msg_command_outgoing", 6}
        },
        0
    }},
    {"minecraft:enchantment", {
        {
            {"density", 0},
            {"impaling", 1},
            {"sweeping_edge", 2},
            {"breach", 3},
            {"silk_touch", 4},
            {"smite", 5},
            {"sharpness", 6},
            {"punch", 7},
            {"bane_of_arthropods", 8},
            {"projectile_protection", 9},
            {"binding_curse", 10},
            {"wind_burst", 11},
            {"loyalty", 12},
            {"multishot", 13},
            {"looting", 14},
            {"lure", 15},
            {"swift_sneak", 16},
            {"protection", 17},
            {"fortune", 18},
            {"efficiency", 19},
            {"unbreaking", 20},
            {"fire_aspect", 21},
            {"piercing", 22},
            {"aqua_affinity", 23},
            {"mending", 24},
            {"infinity", 25},
            {"vanishing_curse", 26},
            {"fire_protection", 27},
            {"blast_protection", 28},
            {"quick_charge", 29},
            {"power", 30},
            {"feather_falling", 31},
            {"luck_of_the_sea", 32},
            {"frost_walker", 33},
            {"flame", 34},
            {"soul_speed", 35},
            {"respiration", 36},
            {"thorns", 37},
            {"riptide", 38},
            {"channeling", 39},
            {"depth_strider", 40},
            {"knockback", 41}
        },
        0
    }},
    {"minecraft:trim_material", {
        {
            {"iron", 0},
            {"redstone", 1},
            {"emerald", 2},
            {"amethyst", 3},
            {"lapis", 4},
            {"quartz", 5},
            {"resin", 6},
            {"diamond", 7},
            {"copper", 8},
            {"netherite", 9},
            {"gold", 10}
        },
        0
    }},
    {"minecraft:cow_variant", {
        {
            {"warm", 0},
            {"temperate", 1},
            {"cold", 2}
        },
        0
    }},
    {"minecraft:jukebox_song", {
        {
            {"blocks", 0},
            {"11", 1},
            {"5", 2},
            {"wait", 3},
            {"far", 4},
            {"ward", 5},
            {"precipice", 6},
            {"relic", 7},
            {"strad", 8},
            {"creator_music_box", 9},
            {"pigstep", 10},
            {"cat", 11},
            {"mall", 12},
            {"stal", 13},
            {"creator", 14},
            {"chirp", 15},
            {"13", 16},
            {"otherside", 17},
            {"mellohi", 18}
        },
        0
    }},
    {"minecraft:test_environment", {
        {
            {"default", 0}
        },
        0
    }},
    {"minecraft:damage_type", {
        {
            {"arrow", 0},
            {"cramming", 1},
            {"player_attack", 2},
            {"stalagmite", 3},
            {"indirect_magic", 4},
            {"mob_attack", 5},
            {"outside_border", 6},
            {"falling_anvil", 7},
            {"freeze", 8},
            {"player_explosion", 9},
            {"dry_out", 10},
            {"generic_kill", 11},
            {"cactus", 12},
            {"wind_charge", 13},
            {"in_wall", 14},
            {"out_of_world", 15},
            {"mob_attack_no_aggro", 16},
            {"fall", 17},
            {"magic", 18},
            {"spit", 19},
            {"campfire", 20},
            {"in_fire", 21},
            {"fly_into_wall", 22},
            {"sting", 23},
            {"drown", 24},
            {"explosion", 25},
            {"wither", 26},
            {"trident", 27},
            {"falling_block", 28},
            {"falling_stalactite", 29},
            {"fireball", 30},
            {"fireworks", 31},
            {"sweet_berry_bush", 32},
            {"lightning_bolt", 33},
            {"sonic_boom", 34},
            {"bad_respawn_point", 35},
            {"on_fire", 36},
            {"dragon_breath", 37},
            {"mace_smash", 38},
            {"thrown", 39},
            {"ender_pearl", 40},
            {"starve", 41},
            {"hot_floor", 42},
            {"thorns", 43},
            {"unattributed_fireball", 44},
            {"generic", 45},
            {"lava", 46},
            {"mob_projectile", 47},
            {"wither_skull", 48}
        },
        0
    }},
    {"minecraft:trim_pattern", {
        {
            {"coast", 0},
            {"spire", 1},
            {"tide", 2},
            {"raiser", 3},
            {"bolt", 4},
            {"vex", 5},
            {"dune", 6},
            {"ward", 7},
            {"host", 8},
            {"flow", 9},
            {"snout", 10},
            {"wild", 11},
            {"shaper", 12},
            {"eye", 13},
            {"rib", 14},
            {"wayfinder", 15},
            {"silence", 16},
            {"sentry", 17}
        },
        0
    }},
    {"minecraft:cat_variant", {
        {
            {"white", 0},
            {"tabby", 1},
            {"ragdoll", 2},
            {"jellie", 3},
            {"siamese", 4},
            {"calico", 5},
            {"persian", 6},
            {"black", 7},
            {"british_shorthair", 8},
            {"all_black", 9},
            {"red", 10}
        },
        0
    }},
    {"minecraft:chicken_variant", {
        {
            {"warm", 0},
            {"temperate", 1},
            {"cold", 2}
        },
        0
    }},
    {"minecraft:frog_variant", {
        {
            {"warm", 0},
            {"temperate", 1},
            {"cold", 2}
        },
        0
    }},
    {"minecraft:banner_pattern", {
        {
            {"triangle_bottom", 0},
            {"square_top_left", 1},
            {"straight_cross", 2},
            {"square_bottom_right", 3},
            {"stripe_top", 4},
            {"diagonal_right", 5},
            {"half_vertical", 6},
            {"gradient", 7},
            {"square_bottom_left", 8},
            {"diagonal_up_right", 9},
            {"stripe_downright", 10},
            {"creeper", 11},
            {"circle", 12},
            {"stripe_left", 13},
            {"piglin", 14},
            {"diagonal_up_left", 15},
            {"stripe_center", 16},
            {"border", 17},
            {"bricks", 18},
            {"cross", 19},
            {"triangles_top", 20},
            {"flower", 21},
            {"triangles_bottom", 22},
            {"skull", 23},
            {"flow", 24},
            {"square_top_right", 25},
            {"triangle_top", 26},
            {"base", 27},
            {"diagonal_left", 28},
            {"mojang", 29},
            {"half_horizontal", 30},
            {"gradient_up", 31},
            {"curly_border", 32},
            {"rhombus", 33},
            {"stripe_downleft", 34},
            {"small_stripes", 35},
            {"globe", 36},
            {"half_vertical_right", 37},
            {"stripe_right", 38},
            {"stripe_middle", 39},
            {"stripe_bottom", 40},
            {"half_horizontal_bottom", 41},
            {"guster", 42}
        },
        0
    }},
    {"minecraft:test_instance", {
        {
            {"always_pass", 0}
        },
        0
    }},
    {"minecraft:wolf_sound_variant", {
        {
            {"big", 0},
            {"puglin", 1},
            {"classic", 2},
            {"grumpy", 3},
            {"sad", 4},
            {"angry", 5},
            {"cute", 6}
        },
        0
    }},
    {"minecraft:painting_variant", {
        {
            {"passage", 0},
            {"sunset", 1},
            {"skull_and_roses", 2},
            {"donkey_kong", 3},
            {"wanderer", 4},
            {"alban", 5},
            {"earth", 6},
            {"sea", 7},
            {"fighters", 8},
            {"finding", 9},
            {"courbet", 10},
            {"creebet", 11},
            {"endboss", 12},
            {"orb", 13},
            {"water", 14},
            {"wind", 15},
            {"unpacked", 16},
            {"burning_skull", 17},
            {"fern", 18},
            {"match", 19},
            {"graham", 20},
            {"kebab", 21},
            {"prairie_ride", 22},
            {"humble", 23},
            {"aztec", 24},
            {"aztec2", 25},
            {"pond", 26},
            {"bomb", 27},
            {"tides", 28},
            {"bust", 29},
            {"wither", 30},
            {"void", 31},
            {"backyard", 32},
            {"baroque", 33},
            {"owlemons", 34},
            {"meditative", 35},
            {"sunflowers", 36},
            {"stage", 37},
            {"bouquet", 38},
            {"pool", 39},
            {"cavebird", 40},
            {"lowmist", 41},
            {"skeleton", 42},
            {"plant", 43},
            {"pigscene", 44},
            {"wasteland", 45},
            {"pointer", 46},
            {"cotan", 47},
            {"changing", 48},
            {"fire", 49}
        },
        0
    }},
    {"minecraft:instrument", {
        {
            {"call_goat_horn", 0},
            {"feel_goat_horn", 1},
            {"dream_goat_horn", 2},
            {"admire_goat_horn", 3},
            {"yearn_goat_horn", 4},
            {"ponder_goat_horn", 5},
            {"sing_goat_horn", 6},
            {"seek_goat_horn", 7}
        },
        0
    }},
    {"minecraft:pig_variant", {
        {
            {"warm", 0},
            {"temperate", 1},
            {"cold", 2}
        },
        0
    }},
    {"minecraft:dimension_type", {
        {
            {"overworld_caves", 0},
            {"the_nether", 1},
            {"the_end", 2},
            {"overworld", 3}
        },
        0
    }},
    {"minecraft:wolf_variant", {
        {
            {"pale", 0},
            {"woods", 1},
            {"snowy", 2},
            {"striped", 3},
            {"chestnut", 4},
            {"black", 5},
            {"ashen", 6},
            {"spotted", 7},
            {"rusty", 8}
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
