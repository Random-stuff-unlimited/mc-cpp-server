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
    resources/gamedata/block_states.json       per block state (index = state id): light_emission, requires_correct_tool, occludes

blocks.json also holds each block's properties (destroy_time, explosion_resistance, friction, speed_factor,
jump_factor, and shape: single, double_height for doors/tall plants, double_length for beds). These and block_states.json come from the game's code, not from the reports:
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
        return json.load(open(out))
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
    state_count = 1 + max(max(s[0] for s in b.get("states", [[b["default"]]])) for b in blocks.values())
    for name, values in extracted["states"].items():
        if len(values) != state_count:
            sys.exit(f"block_states.json: {name} has {len(values)} values for {state_count} states")
    write("block_states.json", extracted["states"])
    overrides = OUT_DIR / "overrides.json"
    if not overrides.exists():
        overrides.write_text(json.dumps({"blocks": {}, "block_items": {}}, indent=2) + "\n")
    write_packet_ids(json.load(open(reports / "packets.json")), info["name"])

    state_count = sum(len(b.get("states", [0])) for b in blocks.values())
    print(f"resources/gamedata/ updated for {info['name']} (protocol {info['protocol_version']}): "
          f"{len(static)} static registries, {len(synced)} synced registries, "
          f"{sum(len(t) for t in tags.values())} tags, {len(blocks)} blocks, {state_count} block states")


if __name__ == "__main__":
    main()
