#!/usr/bin/env python3
"""Regenerate resources/gamedata/ for a Minecraft version.

    python3 tools/update_gamedata.py 1.21.10

Downloads the official server jar, runs Mojang's data generator (needs Java 21+)
and writes compact JSON files the server loads at startup:

    resources/gamedata/version.json            name, protocol version, data version
    resources/gamedata/registries.json         static registries: entry names ordered by id (ids are fixed in the client)
    resources/gamedata/synced_registries.json  registries sent in Registry Data: entry names in send order (the order defines the ids)
    resources/gamedata/tags.json               tags of every registry the client receives, as entry names
    resources/gamedata/blocks.json             block states: default state id and id of every property combination
    resources/gamedata/dimensions.json         min_y and height of every dimension type
    resources/gamedata/block_items.json        item -> block it places ("minecraft:redstone" -> "minecraft:redstone_wire")
    resources/gamedata/block_states.json       per block state (index = state id): light_emission, requires_correct_tool, occludes,
                                               light_block (light absorbed, 0-15), propagates_skylight_down, blocks_motion,
                                               solid, replaceable, random_ticking, liquid, solid_render, use_shape_for_light_occlusion,
                                               ignited_by_lava, analog_output (comparators read it), occlusion_shape (index in collision_shapes.json), redstone_conductor, face_sturdy (bit
                                               direction * 3 + support type FULL/CENTER/RIGID), collision_shape (index in
                                               collision_shapes.json), fluid (minecraft:fluid id), fluid_amount, fluid_falling, push_reaction
                                               (piston: 0 normal, 1 destroy, 2 block, 3 ignore, 4 push only)
    resources/gamedata/collision_shapes.json   collision shapes: lists of boxes [minX, minY, minZ, maxX, maxY, maxZ]
    resources/gamedata/outline_shapes.json     outline shapes (BlockState.getShape, what the cursor and ray casts hit):
                                               {"states": [index in shapes per state id], "shapes": [lists of boxes]}
    resources/gamedata/damage_types.json       damage types (exhaustion, scaling...), as in the game's data
    resources/gamedata/block_loot_tables.json  loot table of each block (what it drops), as in the game's data
    resources/gamedata/tree_features.json      trees (configured features of type minecraft:tree), as in the game's data
    resources/gamedata/recipes.json            recipes, as in the game's data
    resources/gamedata/items.json              per item: max_stack_size, equipment_slot, tool_rules, can_destroy_blocks_in_creative, fire_resistant, crafting_remainder,
                                               food, consumable, use_remainder (components as in the reports), equip_sound and swappable (equippable),
                                               blocks_attacks and combat stats (attack_damage,
                                               attack_speed, armor, armor_toughness, knockback_resistance: bonuses
                                               given while the item is in its slot, from the reports)

blocks.json also holds each block's properties (destroy_time, explosion_resistance, friction, speed_factor,
jump_factor, dynamic_shape, block_entity (its block entity type, if any), classes: the block's Java class, superclasses and interfaces, and shape: single, double_height
for doors/tall plants, double_length for beds). These and block_states.json come from the game's code, not from the reports:
tools/GameDataExtractor.java reads them from the server jar (translated with Mojang's official mappings).

resources/gamedata/overrides.json is written by hand and never overwritten: the server applies it on top of the
generated files, so tweaks (a block's hardness, light...) survive version updates.

and include/network/PacketIds.hpp with the packet ids of this version.

The jar and generator output are cached in .cache/gamedata/<version>/.
"""
import json
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

MANIFEST_URL = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"

# Dynamic registries the client expects in Registry Data (RegistrySynchronization.NETWORKABLE_REGISTRIES).
# Not listed in the generator reports: if a new version crashes with "Missing registry: X", add X here.
SYNCED_REGISTRIES = [
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
    "minecraft:painting_variant",
]

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "resources" / "gamedata"
PACKET_IDS_HEADER = ROOT / "include" / "network" / "PacketIds.hpp"


def fetch_json(url):
    with urllib.request.urlopen(url) as r:
        return json.load(r)


def download(url, path):
    print(f"Downloading {url}")
    with urllib.request.urlopen(url) as r, open(path, "wb") as f:
        shutil.copyfileobj(r, f)


def download_server(version, cache):
    """Server jar, and Mojang's mappings when the jar is obfuscated (None otherwise)"""
    jar, mappings = cache / "server.jar", cache / "server_mappings.txt"
    if jar.exists():
        return jar, (mappings if mappings.exists() else None)
    manifest = fetch_json(MANIFEST_URL)
    entry = next((v for v in manifest["versions"] if v["id"] == version), None)
    if entry is None:
        sys.exit(f"Unknown Minecraft version: {version}")
    downloads = fetch_json(entry["url"])["downloads"]
    cache.mkdir(parents=True, exist_ok=True)
    if "server_mappings" in downloads:
        download(downloads["server_mappings"]["url"], mappings)
    download(downloads["server"]["url"], jar)
    return jar, (mappings if mappings.exists() else None)


def extract_from_code(cache, mappings, version):
    """Runs tools/GameDataExtractor.java on the server jar (the data generator run unpacked it and its libraries)"""
    out = cache / "extracted.json"
    if out.exists():
        cached = json.load(open(out))
        if "outline_shapes" in cached and "entity_types" in cached:  # Else made by an older extractor: run again
            return cached
    jars = [cache / "versions" / version / f"server-{version}.jar"] + sorted((cache / "libraries").rglob("*.jar"))
    print("Extracting values from the game code...")
    subprocess.run(["java", "-cp", ":".join(str(j) for j in jars), str(ROOT / "tools" / "GameDataExtractor.java"),
                    str(mappings) if mappings else "-", str(out)], cwd=cache, check=True, stdout=subprocess.DEVNULL)
    return json.load(open(out))


def run_generator(jar, cache):
    generated = cache / "generated"
    if (generated / "reports" / "registries.json").exists():
        return generated
    print("Running the data generator...")
    subprocess.run(
        ["java", "-DbundlerMainClass=net.minecraft.data.Main", "-jar", jar.name, "--output", "generated", "--server", "--reports"],
        cwd=cache, check=True, stdout=subprocess.DEVNULL)
    return generated


def registry_folder(base, registry):
    return base / registry.split(":", 1)[1]


def entry_names(folder):
    return sorted("minecraft:" + f.relative_to(folder).with_suffix("").as_posix() for f in folder.rglob("*.json"))


def resolve_tag(registry, name, raw, known, stack=()):
    if name in stack:
        sys.exit(f"Tag cycle in {registry}: {name}")
    out = []
    for value in raw[name]:
        ident, required = (value["id"], value.get("required", True)) if isinstance(value, dict) else (value, True)
        if ident.startswith("#"):
            ref = ident[1:]
            if ref in raw:
                out += resolve_tag(registry, ref, raw, known, stack + (name,))
            elif required:
                sys.exit(f"{registry} tag {name}: unknown tag {ref}")
        elif ident in known:
            out.append(ident)
        elif required:
            sys.exit(f"{registry} tag {name}: unknown entry {ident}")
    return list(dict.fromkeys(out))


def detect_shape(properties):
    """Blocks spanning two positions: one state per position, linked by a property"""
    if sorted(properties.get("half", [])) == ["lower", "upper"]:
        return "double_height"  # Doors, tall plants (stairs/trapdoors use half=bottom/top: one block)
    if sorted(properties.get("part", [])) == ["foot", "head"]:
        return "double_length"  # Beds, the head being in the facing direction
    return "single"


ITEM_ATTRIBUTES = {
    "minecraft:attack_damage": "attack_damage",
    "minecraft:attack_speed": "attack_speed",
    "minecraft:armor": "armor",
    "minecraft:armor_toughness": "armor_toughness",
    "minecraft:knockback_resistance": "knockback_resistance",
}


def item_properties(components):
    """Stats of an item from its default components: attribute bonuses apply while it is held or worn"""
    slot = components.get("minecraft:equippable", {}).get("slot", "mainhand")
    props = {"max_stack_size": components.get("minecraft:max_stack_size", 64), "equipment_slot": slot}
    # Which blocks it mines and drops (tool rules: blocks as a block, a list or a #tag, correct_for_drops optional)
    tool = components.get("minecraft:tool")
    if tool:
        props["tool_rules"] = [{k: v for k, v in rule.items() if k in ("blocks", "correct_for_drops")} for rule in tool["rules"]]
        # Swords, the mace, the trident: they don't break blocks in creative
        if tool.get("can_destroy_blocks_in_creative") is False:
            props["can_destroy_blocks_in_creative"] = False
    # Eating and drinking: the components as they are (FoodProperties, Consumable, UseRemainder)
    for component in ("food", "consumable", "use_remainder"):
        if "minecraft:" + component in components:
            props[component] = components["minecraft:" + component]
    # Right click to wear it (Equippable.swappable, true by default) and the sound then
    equippable = components.get("minecraft:equippable")
    if equippable:
        props["swappable"] = equippable.get("swappable", True)
        props["equip_sound"] = equippable.get("equip_sound", "minecraft:item.armor.equip_generic")
    # Shields: used (raised) until released
    if "minecraft:blocks_attacks" in components:
        props["blocks_attacks"] = True
    # Survives fire and lava as an item entity (netherite...)
    if "minecraft:damage_resistant" in components:
        props["fire_resistant"] = True
    for modifier in components.get("minecraft:attribute_modifiers", []):
        name = ITEM_ATTRIBUTES.get(modifier["type"])
        if name and modifier["operation"] == "add_value" and modifier.get("slot") in (slot, "any", "hand", "armor"):
            props[name] = round(props.get(name, 0) + modifier["amount"], 4)
    return props


def write(name, data):
    with open(OUT_DIR / name, "w") as f:
        json.dump(data, f, separators=(",", ":"), sort_keys=False)
        f.write("\n")


def write_packet_ids(packets, version_name):
    """PacketId::<State>::<Direction>::<NAME>, NAME being Mojang's packet name (minecraft:level_chunk_with_light -> LEVEL_CHUNK_WITH_LIGHT)."""
    lines = [
        "#ifndef PACKET_IDS_HPP",
        "#define PACKET_IDS_HPP",
        "",
        f"// Generated by tools/update_gamedata.py for Minecraft {version_name} - do not modify",
        "",
        "#include <cstdint>",
        "",
        "namespace PacketId {",
    ]
    for state in ["handshake", "status", "login", "configuration", "play"]:
        lines.append(f"\tnamespace {state.capitalize()} {{")
        for direction in ["clientbound", "serverbound"]:
            entries = packets.get(state, {}).get(direction, {})
            if not entries:
                continue
            lines.append(f"\t\tnamespace {direction.capitalize()} {{")
            for name, info in sorted(entries.items(), key=lambda e: e[1]["protocol_id"]):
                const = name.split(":", 1)[1].replace("/", "_").upper()
                lines.append(f"\t\t\tconstexpr int32_t {const} = 0x{info['protocol_id']:02X};")
            lines.append("\t\t}")
        lines.append("\t}")
    lines += ["} // namespace PacketId", "", "#endif", ""]
    PACKET_IDS_HEADER.write_text("\n".join(lines))


def write_entity_data(extracted, data, reports):
    """entity_types.json (attributes, then per entity type: size, tracking, classes, default attributes, spawn egg) and
    entity_loot_tables.json (data/minecraft/loot_table/entities/, by table name)"""
    types = extracted["entity_types"]
    for name, content in json.load(open(reports / "items.json")).items():
        entity = content["components"].get("minecraft:entity_data", {}).get("id")
        if name.endswith("_spawn_egg") and entity in types:
            types[entity]["spawn_egg"] = name
    write("entity_types.json", {"attributes": extracted["attributes"], "types": types})
    folder = data / "loot_table" / "entities"
    loot = {"minecraft:entities/" + f.relative_to(folder).with_suffix("").as_posix(): json.load(open(f))
            for f in sorted(folder.rglob("*.json"))}
    write("entity_loot_tables.json", loot)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    version = sys.argv[1]
    cache = ROOT / ".cache" / "gamedata" / version
    jar, mappings = download_server(version, cache)
    generated = run_generator(jar, cache)
    extracted = extract_from_code(cache, mappings, version)
    reports = generated / "reports"
    data = generated / "data" / "minecraft"

    with zipfile.ZipFile(jar) as z:
        info = json.loads(z.read("version.json"))

    # Static registries: list index == protocol id
    static = {}
    for registry, content in json.load(open(reports / "registries.json")).items():
        by_id = {e["protocol_id"]: name for name, e in content["entries"].items()}
        if sorted(by_id) != list(range(len(by_id))):
            sys.exit(f"{registry}: protocol ids are not contiguous")
        static[registry] = [by_id[i] for i in range(len(by_id))]

    # Synced registries: list index == id the client will assign
    synced = {}
    for registry in SYNCED_REGISTRIES:
        folder = registry_folder(data, registry)
        if not folder.is_dir():
            sys.exit(f"{registry}: no data in the generator output, remove it from SYNCED_REGISTRIES?")
        synced[registry] = entry_names(folder)

    # Tags of every registry the client knows: all static ones plus the synced ones
    tags = {}
    for registry, entries in list(static.items()) + list(synced.items()):
        folder = registry_folder(data / "tags", registry)
        if not folder.is_dir():
            continue
        raw = {"minecraft:" + f.relative_to(folder).with_suffix("").as_posix(): json.load(open(f))["values"]
               for f in folder.rglob("*.json")}
        known = set(entries)
        tags[registry] = {name: resolve_tag(registry, name, raw, known) for name in sorted(raw)}

    # Block states
    blocks = {}
    for block, content in json.load(open(reports / "blocks.json")).items():
        default = next(s["id"] for s in content["states"] if s.get("default"))
        entry = {"default": default, "properties": {**extracted["blocks"][block], "shape": detect_shape(content.get("properties", {}))}}
        if "properties" in content:
            entry["states"] = [[s["id"], s["properties"]] for s in content["states"]]
        blocks[block] = entry

    dimensions = {}
    for f in sorted((data / "dimension_type").glob("*.json")):
        content = json.load(open(f))
        dimensions["minecraft:" + f.stem] = {"min_y": content["min_y"], "height": content["height"]}

    OUT_DIR.mkdir(exist_ok=True)
    write("version.json", {"name": info["name"], "protocol": info["protocol_version"], "data_version": info["world_version"]})
    write("registries.json", static)
    write("synced_registries.json", synced)
    write("tags.json", tags)
    write("blocks.json", blocks)
    write("dimensions.json", dimensions)
    write("block_items.json", extracted["block_items"])
    items = {name: item_properties(content["components"]) for name, content in json.load(open(reports / "items.json")).items()}
    for name, remainder in extracted.get("crafting_remainders", {}).items():
        items[name]["crafting_remainder"] = remainder
    write("items.json", items)
    state_count = 1 + max(max(s[0] for s in b.get("states", [[b["default"]]])) for b in blocks.values())
    for name, values in extracted["states"].items():  # outline_shape included
        if len(values) != state_count:
            sys.exit(f"block_states.json: {name} has {len(values)} values for {state_count} states")
    states = dict(extracted["states"])
    outline = states.pop("outline_shape")
    write("block_states.json", states)
    write("collision_shapes.json", extracted["collision_shapes"])
    write("outline_shapes.json", {"states": outline, "shapes": extracted["outline_shapes"]})
    # Damage types (exhaustion caused, difficulty scaling...), by name
    damage_types = {"minecraft:" + f.stem: json.load(open(f)) for f in sorted((data / "damage_type").glob("*.json"))}
    write("damage_types.json", damage_types)
    # What each block drops: its loot table (data/minecraft/loot_table/blocks/<block>.json), by block
    loot = {"minecraft:" + f.stem: json.load(open(f)) for f in sorted((data / "loot_table" / "blocks").glob("*.json"))}
    write("block_loot_tables.json", loot)
    # Trees (configured features of type minecraft:tree), for saplings
    trees = {}
    for f in sorted((data / "worldgen" / "configured_feature").glob("*.json")):
        feature = json.load(open(f))
        if feature.get("type") == "minecraft:tree":
            trees["minecraft:" + f.stem] = feature["config"]
    write("tree_features.json", trees)
    # Recipes (crafting, cooking, stonecutting, smithing...), by name
    recipes = {"minecraft:" + f.stem: json.load(open(f)) for f in sorted((data / "recipe").glob("*.json"))}
    write("recipes.json", recipes)
    write_entity_data(extracted, data, reports)
    overrides = OUT_DIR / "overrides.json"
    if not overrides.exists():
        overrides.write_text(json.dumps({"blocks": {}, "items": {}, "block_items": {}}, indent=2) + "\n")
    write_packet_ids(json.load(open(reports / "packets.json")), info["name"])

    state_count = sum(len(b.get("states", [0])) for b in blocks.values())
    print(f"resources/gamedata/ updated for {info['name']} (protocol {info['protocol_version']}): "
          f"{len(static)} static registries, {len(synced)} synced registries, "
          f"{sum(len(t) for t in tags.values())} tags, {len(blocks)} blocks, {state_count} block states")


if __name__ == "__main__":
    main()
