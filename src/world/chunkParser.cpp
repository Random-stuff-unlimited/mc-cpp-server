#include "lib/nbtParser.hpp"
#include "world.hpp"
#include "world/chunkParser.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <zlib.h>

World::DirectChunkParser::DirectChunkParser(const ParseConfig& config) : _config(config) {
	if (_config.worldHeight <= 0) {
		throw std::invalid_argument("World height must be positive");
	}
	if (_config.cacheSize == 0) {
		_config.cacheSize = 1; // Minimum cache size
	}
}

World::DirectChunkParser::~DirectChunkParser() {
	// Clean up region file cache
	_regionCache.clear();
}

World::ChunkData World::DirectChunkParser::parseChunkFromRegion(const std::filesystem::path& regionPath, int32_t chunkX, int32_t chunkZ) {

	// Load region file
	RegionFile* region = loadRegionFile(regionPath);
	if (!region) {
		throw std::runtime_error("Failed to load region file: " + regionPath.string());
	}

	// Read chunk data
	std::vector<uint8_t> chunkData = readChunkData(region, chunkX, chunkZ);
	if (chunkData.empty()) {
		// Return empty chunk if not found
		g_logger->logGameInfo(INFO,
							  "No chunk data found, returning empty chunk for (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ")",
							  "CHUNK_PARSER");
		return World::ChunkData::generateEmpty(chunkX, chunkZ, _config.worldHeight);
	}

	g_logger->logGameInfo(INFO,
						  "Found chunk data (" + std::to_string(chunkData.size()) + " bytes) for chunk (" + std::to_string(chunkX) + ", " +
								  std::to_string(chunkZ) + ")",
						  "CHUNK_PARSER");

	// Parse NBT data using your NBT parser
	nbt::Parser parser;
	nbt::NBT	chunkNBT = parser.parse(chunkData);

	return parseChunkFromNBT(chunkNBT, chunkX, chunkZ);
}

World::ChunkData World::DirectChunkParser::parseChunkFromNBT(const nbt::NBT& chunkNBT, int32_t chunkX, int32_t chunkZ) {
	// DEBUG: Log entry point
	g_logger->logGameInfo(
			INFO, "parseChunkFromNBT called for chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ")", "CHUNK_PARSER");

	World::ChunkData chunk(chunkX, chunkZ, _config.worldHeight, _config.minY);

	// Get the root compound - your NBT library stores data directly in root
	const auto& root = chunkNBT.getRoot();

	try {
		// For 1.18+ format, data is directly in root
		// For pre-1.18, check if there's a "Level" tag
		const nbt::TagCompound* level = &root;
		if (root.contains("Level")) {
			// Pre-1.18 format
			level = std::get<std::shared_ptr<nbt::TagCompound>>(root.at("Level").data).get();
		}

		// Parse chunk metadata
		parseChunkMetadata(*level, chunk);

		// Parse heightmaps
		if (hasTag(*level, "Heightmaps")) {
			const auto& heightmapsTag = std::get<std::shared_ptr<nbt::TagCompound>>(level->at("Heightmaps").data);
			parseHeightMaps(*heightmapsTag, chunk);
		}

		// Parse sections
		if (hasTag(*level, "Sections") || hasTag(*level, "sections")) {
			std::string sectionsKey = hasTag(*level, "Sections") ? "Sections" : "sections";
			const auto& sectionsTag = std::get<std::shared_ptr<nbt::TagList>>(level->at(sectionsKey).data);
			parseSections(*sectionsTag, chunk);
		}

		// Parse block entities
		if (_config.loadBlockEntities && (hasTag(*level, "BlockEntities") || hasTag(*level, "block_entities"))) {
			std::string blockEntitiesKey = hasTag(*level, "BlockEntities") ? "BlockEntities" : "block_entities";
			const auto& blockEntitiesTag = std::get<std::shared_ptr<nbt::TagList>>(level->at(blockEntitiesKey).data);
			parseBlockEntities(*blockEntitiesTag, chunk);
		}

		// Validate if requested
		if (_config.validateData) {
			validateChunkData(chunk);
		}

	} catch (const std::exception& e) {
		throw std::runtime_error("Failed to parse chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + "): " + e.what());
	}

	return chunk;
}

void World::DirectChunkParser::parseChunkMetadata(const nbt::TagCompound& level, World::ChunkData& chunk) {
	chunk.inhabitedTime = getTagValue<int64_t>(level, "InhabitedTime", 0);
	chunk.lastUpdate	= getTagValue<int64_t>(level, "LastUpdate", 0);

	// Check generation status
	std::string status	   = getTagValue<std::string>(level, "Status", "empty");
	chunk.isFullyGenerated = (status == "full" || status == "fullchunk");
}

void World::DirectChunkParser::parseHeightMaps(const nbt::TagCompound& heightmaps, World::ChunkData& chunk) {
	// Parse each available heightmap type
	std::vector<std::pair<std::string, World::Type>> heightmapTypes = {{"MOTION_BLOCKING", World::Type::MOTION_BLOCKING},
																	   {"MOTION_BLOCKING_NO_LEAVES", World::Type::MOTION_BLOCKING_NO_LEAVES},
																	   {"OCEAN_FLOOR", World::Type::OCEAN_FLOOR},
																	   {"WORLD_SURFACE", World::Type::WORLD_SURFACE},
																	   {"WORLD_SURFACE_WG", World::Type::WORLD_SURFACE_WG}};

	for (const auto& [name, type] : heightmapTypes) {
		if (hasTag(heightmaps, name)) {
			const auto& longArrayData = std::get<nbt::TagLongArray>(heightmaps.at(name).data);
			chunk.heightmaps.setHeightMap(type, longArrayData);
		}
	}
}

void World::DirectChunkParser::parseSections(const nbt::TagList& sections, World::ChunkData& chunk) {
	g_logger->logGameInfo(INFO, "parseSections called with " + std::to_string(sections.size()) + " sections", "CHUNK_PARSER");

	for (size_t i = 0; i < sections.size(); ++i) {
		const auto& sectionTag = std::get<std::shared_ptr<nbt::TagCompound>>(sections[i].data);

		// Get section Y coordinate
		int8_t sectionY = getTagValue<int8_t>(*sectionTag, "Y", 0);

		// Calculate section index
		int sectionIndex = sectionY - (_config.minY / 16);
		if (sectionIndex < 0 || sectionIndex >= static_cast<int>(chunk.sections.size())) {
			continue; // Skip invalid sections
		}

		World::ChunkSection& section = chunk.sections[sectionIndex];
		section.isEmpty				 = false;

		// DEBUG: Log section being parsed
		g_logger->logGameInfo(
				INFO, "Parsing section Y=" + std::to_string(sectionY) + " (index=" + std::to_string(sectionIndex) + ")", "CHUNK_PARSER");

		// Parse block states
		if (hasTag(*sectionTag, "block_states")) {
			const auto& blockStatesTag = std::get<std::shared_ptr<nbt::TagCompound>>(sectionTag->at("block_states").data);
			g_logger->logGameInfo(INFO, "Found block_states in section " + std::to_string(sectionY), "CHUNK_PARSER");
			parseBlockStates(*blockStatesTag, section);
		} else {
			g_logger->logGameInfo(INFO, "No block_states found in section " + std::to_string(sectionY), "CHUNK_PARSER");
		}

		// Parse biomes
		if (hasTag(*sectionTag, "biomes")) {
			const auto& biomesTag = std::get<std::shared_ptr<nbt::TagCompound>>(sectionTag->at("biomes").data);
			parseBiomes(*biomesTag, section);
		}

		// Parse lighting if enabled
		if (_config.loadLighting) {
			parseLighting(*sectionTag, section);
		}
	}
}

void World::DirectChunkParser::parseBlockStates(const nbt::TagCompound& blockStates, World::ChunkSection& section) {
	std::vector<uint32_t> blockData;

	// DEBUG: Log what tags are present in block_states
	std::string availableTags = "Available tags in block_states: ";
	for (const auto& [key, value] : blockStates) {
		availableTags += key + " ";
	}
	g_logger->logGameInfo(INFO, availableTags, "CHUNK_PARSER");

	if (hasTag(blockStates, "data") && hasTag(blockStates, "palette")) {
		// Indirect encoding with palette
		const auto& dataArray = std::get<nbt::TagLongArray>(blockStates.at("data").data);
		const auto& palette	  = std::get<std::shared_ptr<nbt::TagList>>(blockStates.at("palette").data);

		g_logger->logGameInfo(INFO, "Processing paletted data - palette size: " + std::to_string(palette->size()), "CHUNK_PARSER");
		blockData = unpackPalettedData(dataArray, palette.get(), 4096, 0);

	} else if (hasTag(blockStates, "data")) {
		// Direct encoding
		const auto& dataArray = std::get<nbt::TagLongArray>(blockStates.at("data").data);
		g_logger->logGameInfo(INFO, "Processing direct encoding - data array size: " + std::to_string(dataArray.size()), "CHUNK_PARSER");
		blockData = unpackDirectPalettedData(dataArray, 15, 4096); // 15 bits for direct block states

	} else if (hasTag(blockStates, "palette")) {
		// Only palette exists - single value blocks (common for air or uniform sections)
		const auto& palette = std::get<std::shared_ptr<nbt::TagList>>(blockStates.at("palette").data);
		g_logger->logGameInfo(INFO, "Processing palette-only data - palette size: " + std::to_string(palette->size()), "CHUNK_PARSER");

		if (palette->size() == 1) {
			// Single block type for entire section
			const auto& paletteEntry = std::get<std::shared_ptr<nbt::TagCompound>>((*palette)[0].data);
			uint32_t	singleValue	 = 0; // Default air block

			if (hasTag(*paletteEntry, "Name")) {
				const auto& blockName = getTagValue<std::string>(*paletteEntry, "Name", "minecraft:air");
				g_logger->logGameInfo(INFO, "Found single block in palette: " + blockName, "CHUNK_PARSER");
				singleValue = getBlockIdFromName(blockName);
				g_logger->logGameInfo(INFO, "Mapped " + blockName + " to ID: " + std::to_string(singleValue), "CHUNK_PARSER");
			}

			blockData.resize(4096, singleValue);
		} else {
			g_logger->logGameInfo(INFO, "Multi-entry palette without data array - defaulting to air", "CHUNK_PARSER");
			blockData.resize(4096, 0);
		}

	} else {
		// No data or palette found
		g_logger->logGameInfo(INFO, "No data or palette found - defaulting to air blocks", "CHUNK_PARSER");
		blockData.resize(4096, 0);
	}

	// Create paletted container
	section.blockStates = std::make_unique<World::PalettedContainer>(4096, true);
	section.blockStates->setFromArray(blockData);

	// Calculate block count
	section.calculateBlockCount(blockData);

	// DEBUG: Log the result
	g_logger->logGameInfo(INFO, "Section parsed - block count: " + std::to_string(section.blockCount), "CHUNK_PARSER");
}

void World::DirectChunkParser::parseBiomes(const nbt::TagCompound& biomes, World::ChunkSection& section) {
	std::vector<uint32_t> biomeData;

	if (hasTag(biomes, "data") && hasTag(biomes, "palette")) {
		// Indirect encoding with palette
		const auto& dataArray = std::get<nbt::TagLongArray>(biomes.at("data").data);
		const auto& palette	  = std::get<std::shared_ptr<nbt::TagList>>(biomes.at("palette").data);

		biomeData = unpackPalettedData(dataArray, palette.get(), 64, 0);

	} else if (hasTag(biomes, "data")) {
		// Direct encoding
		const auto& dataArray = std::get<nbt::TagLongArray>(biomes.at("data").data);
		biomeData			  = unpackDirectPalettedData(dataArray, 6, 64); // 6 bits for direct biomes

	} else {
		// Single value (plains biome)
		biomeData.resize(64, 1); // Plains biome ID
	}

	// Create paletted container
	section.biomes = std::make_unique<World::PalettedContainer>(64, false);
	section.biomes->setFromArray(biomeData);
}

void World::DirectChunkParser::parseLighting(const nbt::TagCompound& sectionTag, World::ChunkSection& section) {
	// Parse sky light
	if (hasTag(sectionTag, "SkyLight")) {
		const auto& skyLight = std::get<nbt::TagByteArray>(sectionTag.at("SkyLight").data);
		section.skyLight	 = std::vector<uint8_t>(skyLight.begin(), skyLight.end());
		section.hasSkyLight	 = true;
	}

	// Parse block light
	if (hasTag(sectionTag, "BlockLight")) {
		const auto& blockLight = std::get<nbt::TagByteArray>(sectionTag.at("BlockLight").data);
		section.blockLight	   = std::vector<uint8_t>(blockLight.begin(), blockLight.end());
		section.hasBlockLight  = true;
	}
}

std::vector<uint32_t>
World::DirectChunkParser::unpackPalettedData(const nbt::TagLongArray& data, const nbt::TagList* palette, size_t expectedSize, uint32_t defaultValue) {

	std::vector<uint32_t> result(expectedSize, defaultValue);

	if (!palette || palette->size() == 0) {
		return result; // All default values
	}

	// Single value optimization
	if (palette->size() == 1) {
		// Extract the palette entry - assuming it's a compound with block state info
		const auto& paletteEntry = std::get<std::shared_ptr<nbt::TagCompound>>((*palette)[0].data);
		uint32_t	singleValue	 = 0; // Default air block

		// DEBUG: Log all tags in this palette entry
		std::string paletteDebug = "Palette entry tags: ";
		for (const auto& [key, value] : *paletteEntry) {
			paletteDebug += key + " ";
		}
		g_logger->logGameInfo(INFO, paletteDebug, "CHUNK_PARSER");

		if (hasTag(*paletteEntry, "Name")) {
			const auto& blockName = getTagValue<std::string>(*paletteEntry, "Name", "minecraft:air");

			// DEBUG: Log what block we're parsing
			g_logger->logGameInfo(INFO, "Found block in palette: " + blockName, "CHUNK_PARSER");

			singleValue = getBlockIdFromName(blockName);

			// DEBUG: Log the result
			g_logger->logGameInfo(INFO, "Mapped " + blockName + " to ID: " + std::to_string(singleValue), "CHUNK_PARSER");
		} else {
			g_logger->logGameInfo(INFO, "No Name tag in palette entry", "CHUNK_PARSER");
			singleValue = 0; // Default to air
		}

		std::fill(result.begin(), result.end(), singleValue);
		return result;
	}

	// DEBUG: Log palette details for multi-entry palettes
	g_logger->logGameInfo(INFO, "Multi-entry palette with " + std::to_string(palette->size()) + " entries:", "CHUNK_PARSER");
	for (size_t p = 0; p < std::min(palette->size(), static_cast<size_t>(5)); ++p) { // Log first 5 entries
		const auto& paletteEntry = std::get<std::shared_ptr<nbt::TagCompound>>((*palette)[p].data);
		if (hasTag(*paletteEntry, "Name")) {
			const auto& blockName = getTagValue<std::string>(*paletteEntry, "Name", "unknown");
			g_logger->logGameInfo(INFO, "  Palette[" + std::to_string(p) + "]: " + blockName, "CHUNK_PARSER");
		}
	}

	// Calculate bits per entry
	uint8_t bitsPerEntry = static_cast<uint8_t>(std::ceil(std::log2(palette->size())));
	bitsPerEntry		 = std::max(bitsPerEntry, static_cast<uint8_t>(4)); // Minimum 4 bits for blocks

	g_logger->logGameInfo(INFO, "Using " + std::to_string(bitsPerEntry) + " bits per entry", "CHUNK_PARSER");

	// Unpack data
	size_t	 valuesPerLong = 64 / bitsPerEntry;
	uint64_t mask		   = (1ULL << bitsPerEntry) - 1;

	for (size_t i = 0; i < expectedSize && i < data.size() * valuesPerLong; ++i) {
		size_t longIndex = i / valuesPerLong;
		size_t bitOffset = (i % valuesPerLong) * bitsPerEntry;

		if (longIndex >= data.size()) break;

		uint64_t longValue	  = static_cast<uint64_t>(data[longIndex]);
		uint32_t paletteIndex = static_cast<uint32_t>((longValue >> bitOffset) & mask);

		if (paletteIndex < palette->size()) {
			// For now, map palette entries to proper block IDs
			const auto& paletteEntry = std::get<std::shared_ptr<nbt::TagCompound>>((*palette)[paletteIndex].data);
			if (hasTag(*paletteEntry, "Name")) {
				const auto& blockName = getTagValue<std::string>(*paletteEntry, "Name", "minecraft:air");
				result[i]			  = getBlockIdFromName(blockName);
			} else {
				result[i] = 0; // Default to air
			}
		}
	}

	return result;
}
uint32_t World::DirectChunkParser::getBlockIdFromName(const std::string& blockName) {
	// Expanded block mapping for common world blocks
	static const std::unordered_map<std::string, uint32_t> BASIC_BLOCKS = {{"minecraft:air", 0},
																		   {"minecraft:stone", 1},
																		   {"minecraft:granite", 2},
																		   {"minecraft:polished_granite", 3},
																		   {"minecraft:diorite", 4},
																		   {"minecraft:polished_diorite", 5},
																		   {"minecraft:andesite", 6},
																		   {"minecraft:polished_andesite", 7},
																		   {"minecraft:grass_block", 8},
																		   {"minecraft:dirt", 9},
																		   {"minecraft:coarse_dirt", 10},
																		   {"minecraft:podzol", 11},
																		   {"minecraft:cobblestone", 12},
																		   {"minecraft:oak_planks", 13},
																		   {"minecraft:spruce_planks", 14},
																		   {"minecraft:birch_planks", 15},
																		   {"minecraft:jungle_planks", 16},
																		   {"minecraft:acacia_planks", 17},
																		   {"minecraft:dark_oak_planks", 18},
																		   {"minecraft:oak_sapling", 19},
																		   {"minecraft:spruce_sapling", 20},
																		   {"minecraft:bedrock", 21},
																		   {"minecraft:water", 22},
																		   {"minecraft:lava", 23},
																		   {"minecraft:sand", 24},
																		   {"minecraft:red_sand", 25},
																		   {"minecraft:gravel", 26},
																		   {"minecraft:gold_ore", 27},
																		   {"minecraft:deepslate_gold_ore", 28},
																		   {"minecraft:iron_ore", 29},
																		   {"minecraft:deepslate_iron_ore", 30},
																		   {"minecraft:coal_ore", 31},
																		   {"minecraft:deepslate_coal_ore", 32},
																		   {"minecraft:nether_gold_ore", 33},
																		   {"minecraft:oak_log", 34},
																		   {"minecraft:spruce_log", 35},
																		   {"minecraft:birch_log", 36},
																		   {"minecraft:jungle_log", 37},
																		   {"minecraft:acacia_log", 38},
																		   {"minecraft:dark_oak_log", 39},
																		   {"minecraft:stripped_spruce_log", 40},
																		   {"minecraft:stripped_birch_log", 41},
																		   {"minecraft:stripped_jungle_log", 42},
																		   {"minecraft:stripped_acacia_log", 43},
																		   {"minecraft:stripped_dark_oak_log", 44},
																		   {"minecraft:stripped_oak_log", 45},
																		   {"minecraft:oak_wood", 46},
																		   {"minecraft:spruce_wood", 47},
																		   {"minecraft:birch_wood", 48},
																		   {"minecraft:jungle_wood", 49},
																		   {"minecraft:acacia_wood", 50},
																		   {"minecraft:dark_oak_wood", 51},
																		   {"minecraft:stripped_oak_wood", 52},
																		   {"minecraft:stripped_spruce_wood", 53}};

	auto it = BASIC_BLOCKS.find(blockName);
	if (it != BASIC_BLOCKS.end()) {
		return it->second;
	}

	// Log unmapped block names so we can add them to our mapping
	g_logger->logGameInfo(INFO, "Unknown block name (defaulting to air): " + blockName, "CHUNK_PARSER");
	return 0; // Default to air if not found
}

std::vector<uint32_t> World::DirectChunkParser::unpackDirectPalettedData(const nbt::TagLongArray& data, uint8_t bitsPerEntry, size_t expectedSize) {

	std::vector<uint32_t> result(expectedSize, 0);

	size_t	 valuesPerLong = 64 / bitsPerEntry;
	uint64_t mask		   = (1ULL << bitsPerEntry) - 1;

	for (size_t i = 0; i < expectedSize && i < data.size() * valuesPerLong; ++i) {
		size_t longIndex = i / valuesPerLong;
		size_t bitOffset = (i % valuesPerLong) * bitsPerEntry;

		if (longIndex >= data.size()) break;

		uint64_t longValue = static_cast<uint64_t>(data[longIndex]);
		result[i]		   = static_cast<uint32_t>((longValue >> bitOffset) & mask);
	}

	return result;
}

void World::DirectChunkParser::parseBlockEntities(const nbt::TagList& blockEntities, World::ChunkData& chunk) {
	for (size_t i = 0; i < blockEntities.size(); ++i) {
		const auto& beTag = std::get<std::shared_ptr<nbt::TagCompound>>(blockEntities[i].data);

		// Get coordinates
		int32_t x = getTagValue<int32_t>(*beTag, "x", 0);
		int32_t y = getTagValue<int32_t>(*beTag, "y", 0);
		int32_t z = getTagValue<int32_t>(*beTag, "z", 0);

		// Convert to chunk-relative coordinates
		uint8_t relX = static_cast<uint8_t>(x & 15);
		uint8_t relZ = static_cast<uint8_t>(z & 15);

		// Get type
		std::string typeStr = getTagValue<std::string>(*beTag, "id", "");

		// Simple hash function for type ID - you'd want a proper mapping
		uint32_t type = 0;
		for (char c : typeStr) {
			type = type * 31 + static_cast<uint32_t>(c);
		}

		// Create block entity
		World::ChunkData::BlockEntity blockEntity(relX, relZ, static_cast<int16_t>(y), type);

		// For now, store empty NBT data - you'd need proper NBT serialization
		blockEntity.nbtData.clear();

		chunk.blockEntities.push_back(std::move(blockEntity));
	}
}

World::DirectChunkParser::RegionFile* World::DirectChunkParser::loadRegionFile(const std::filesystem::path& regionPath) const {
	// Extract region coordinates from filename
	auto filename = regionPath.filename().stem().string();
	// Format: r.x.z.mca

	// Parse region coordinates from filename (format: r.x.z.mca)
	std::pair<int32_t, int32_t> coords{0, 0};

	// Extract coordinates from filename like "r.-1.0.mca"
	size_t firstDot	 = filename.find('.', 2); // Start after "r."
	size_t secondDot = filename.find('.', firstDot + 1);

	if (firstDot != std::string::npos && secondDot != std::string::npos) {
		try {
			coords.first  = std::stoi(filename.substr(2, firstDot - 2));						// regionX
			coords.second = std::stoi(filename.substr(firstDot + 1, secondDot - firstDot - 1)); // regionZ
		} catch (const std::exception& e) {
			// Fall back to (0, 0) if parsing fails
			coords = {0, 0};
		}
	}
	auto it = _regionCache.find(coords);

	if (it != _regionCache.end() && it->second->isLoaded) {
		return it->second.get();
	}

	// Load new region file
	auto region	   = std::make_unique<RegionFile>();
	region->path   = regionPath;
	region->stream = std::make_unique<std::ifstream>(regionPath, std::ios::binary);

	if (!region->stream->is_open()) {
		return nullptr;
	}

	parseRegionHeader(region.get());
	region->isLoaded = true;

	// Cache management
	if (_regionCache.size() >= _config.cacheSize) {
		_regionCache.erase(_regionCache.begin()); // Simple eviction
	}

	RegionFile* ptr		 = region.get();
	_regionCache[coords] = std::move(region);
	return ptr;
}

bool World::DirectChunkParser::chunkExistsInRegion(const std::filesystem::path& regionPath, int32_t chunkX, int32_t chunkZ) {
	RegionFile* region = loadRegionFile(regionPath);
	if (!region) return false;

	size_t index = getChunkIndex(chunkX, chunkZ);
	return index < region->chunkOffsets.size() && region->chunkOffsets[index] != 0;
}

void World::DirectChunkParser::validateChunkData(const World::ChunkData& chunk) const {
	// Validate chunk coordinates
	if (std::abs(chunk.chunkX) > 30000000 || std::abs(chunk.chunkZ) > 30000000) {
		throw std::runtime_error("Chunk coordinates out of valid range");
	}

	// Validate sections
	for (size_t i = 0; i < chunk.sections.size(); ++i) {
		if (!chunk.sections[i].isEmpty) {
			validateSection(chunk.sections[i], static_cast<int>(i) + (_config.minY / 16));
		}
	}
}

void World::DirectChunkParser::validateSection(const World::ChunkSection& section, int sectionY) const {
	// Validate block states
	if (section.blockStates && section.blockStates->getSize() != 4096) {
		throw std::runtime_error("Invalid block states size in section " + std::to_string(sectionY));
	}

	// Validate biomes
	if (section.biomes && section.biomes->getSize() != 64) {
		throw std::runtime_error("Invalid biomes size in section " + std::to_string(sectionY));
	}

	// Validate lighting
	if (section.hasSkyLight && section.skyLight.size() != 2048) {
		throw std::runtime_error("Invalid sky light size in section " + std::to_string(sectionY));
	}

	if (section.hasBlockLight && section.blockLight.size() != 2048) {
		throw std::runtime_error("Invalid block light size in section " + std::to_string(sectionY));
	}
}

// Helper method implementations
template <typename T> T World::DirectChunkParser::getTagValue(const nbt::TagCompound& compound, const std::string& key, T defaultValue) const {
	if (!compound.contains(key)) {
		return defaultValue;
	}

	try {
		return std::get<T>(compound.at(key).data);
	} catch (const std::bad_variant_access&) {
		return defaultValue;
	}
}

bool World::DirectChunkParser::hasTag(const nbt::TagCompound& compound, const std::string& key) const { return compound.contains(key); }

std::pair<int32_t, int32_t> World::DirectChunkParser::getRegionCoords(int32_t chunkX, int32_t chunkZ) const {
	return {chunkX >> 5, chunkZ >> 5}; // Divide by 32
}

size_t World::DirectChunkParser::getChunkIndex(int32_t chunkX, int32_t chunkZ) const {
	int localX = chunkX & 31; // Modulo 32
	int localZ = chunkZ & 31; // Modulo 32
	return static_cast<size_t>(localX + localZ * 32);
}

uint32_t World::DirectChunkParser::readBigEndianInt(const uint8_t* data) const {
	return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) | (static_cast<uint32_t>(data[2]) << 8) |
		   static_cast<uint32_t>(data[3]);
}

// Stub implementations for missing methods
std::vector<uint8_t> World::DirectChunkParser::readChunkData(RegionFile* region, int32_t chunkX, int32_t chunkZ) const {
	if (!region || !region->stream || !region->stream->is_open()) {
		g_logger->logGameInfo(
				INFO, "Region file not open for chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ")", "CHUNK_PARSER");
		return {};
	}

	// Calculate local chunk coordinates within region (0-31)
	int localChunkX = chunkX & 31; // chunkX % 32
	int localChunkZ = chunkZ & 31; // chunkZ % 32

	// Calculate offset table index (chunks are stored in Z-major order)
	int chunkIndex = localChunkX + localChunkZ * 32;

	// Read chunk offset and size from header
	region->stream->seekg(chunkIndex * 4);
	if (region->stream->fail()) {
		return {};
	}

	uint32_t offsetAndSize;
	region->stream->read(reinterpret_cast<char*>(&offsetAndSize), 4);
	if (region->stream->fail()) {
		return {};
	}

	// Convert from big-endian
	offsetAndSize = __builtin_bswap32(offsetAndSize);

	// Extract offset (24 bits) and size (8 bits)
	uint32_t chunkOffset  = (offsetAndSize >> 8) * 4096; // Offset in sectors (4KB each)
	uint8_t	 chunkSectors = offsetAndSize & 0xFF;

	// Check if chunk exists
	if (chunkOffset == 0 || chunkSectors == 0) {
		g_logger->logGameInfo(
				INFO, "Chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + ") not found in region file", "CHUNK_PARSER");
		return {}; // Chunk not generated
	}

	// Read timestamp (we don't need it, but skip it)
	region->stream->seekg(4096 + chunkIndex * 4);
	uint32_t timestamp;
	region->stream->read(reinterpret_cast<char*>(&timestamp), 4);

	// Seek to chunk data
	region->stream->seekg(chunkOffset);
	if (region->stream->fail()) {
		return {};
	}

	// Read chunk data header (length + compression type)
	uint32_t chunkLength;
	uint8_t	 compressionType;

	region->stream->read(reinterpret_cast<char*>(&chunkLength), 4);
	region->stream->read(reinterpret_cast<char*>(&compressionType), 1);

	if (region->stream->fail()) {
		return {};
	}

	// Convert length from big-endian and subtract 1 for compression type byte
	chunkLength = __builtin_bswap32(chunkLength) - 1;

	// Check for valid compression type (1=gzip, 2=zlib, 3=uncompressed)
	if (compressionType < 1 || compressionType > 3) {
		return {};
	}

	// Read compressed chunk data
	std::vector<uint8_t> compressedData(chunkLength);
	region->stream->read(reinterpret_cast<char*>(compressedData.data()), chunkLength);

	if (region->stream->fail()) {
		return {};
	}

	// Decompress the data
	std::vector<uint8_t> decompressedData;

	if (compressionType == 3) {
		// Uncompressed
		decompressedData = compressedData;
	} else {
		// Compressed (zlib or gzip) - simplified decompression
		// For now, return the compressed data - a full zlib implementation would be needed
		// This is a placeholder that needs proper zlib decompression
		try {
			// Basic zlib decompression attempt
			z_stream stream = {};

			// Initialize for zlib (2) or gzip (1)
			int windowBits = (compressionType == 1) ? 15 + 16 : 15; // +16 for gzip

			if (inflateInit2(&stream, windowBits) != Z_OK) {
				return {};
			}

			stream.next_in	= compressedData.data();
			stream.avail_in = chunkLength;

			// Estimate output size (usually 10-20x larger)
			decompressedData.resize(chunkLength * 20);
			stream.next_out	 = decompressedData.data();
			stream.avail_out = decompressedData.size();

			int result = inflate(&stream, Z_FINISH);

			if (result == Z_STREAM_END) {
				decompressedData.resize(stream.total_out);
			} else {
				inflateEnd(&stream);
				return {};
			}

			inflateEnd(&stream);
		} catch (...) {
			return {};
		}
	}

	g_logger->logGameInfo(INFO,
						  "Successfully read and decompressed chunk (" + std::to_string(chunkX) + ", " + std::to_string(chunkZ) + "), " +
								  std::to_string(decompressedData.size()) + " bytes",
						  "CHUNK_PARSER");
	return decompressedData;
}

void World::DirectChunkParser::parseRegionHeader(RegionFile* region) const {
	if (!region || !region->stream || !region->stream->is_open()) {
		return;
	}

	// Region files have 1024 chunks (32x32)
	region->chunkOffsets.resize(1024);
	region->chunkTimestamps.resize(1024);

	// Read chunk location table (first 4KB)
	region->stream->seekg(0);
	for (int i = 0; i < 1024; i++) {
		uint32_t offsetAndSize;
		region->stream->read(reinterpret_cast<char*>(&offsetAndSize), 4);
		if (region->stream->fail()) {
			return;
		}
		// Convert from big-endian and store
		region->chunkOffsets[i] = __builtin_bswap32(offsetAndSize);
	}

	// Read chunk timestamp table (second 4KB)
	for (int i = 0; i < 1024; i++) {
		uint32_t timestamp;
		region->stream->read(reinterpret_cast<char*>(&timestamp), 4);
		if (region->stream->fail()) {
			return;
		}
		// Convert from big-endian and store
		region->chunkTimestamps[i] = __builtin_bswap32(timestamp);
	}
}

std::vector<World::ChunkData> World::DirectChunkParser::parseChunkBatch(const std::filesystem::path&					regionPath,
																		const std::vector<std::pair<int32_t, int32_t>>& coordinates) {

	std::vector<World::ChunkData> chunks;
	chunks.reserve(coordinates.size());

	for (const auto& [chunkX, chunkZ] : coordinates) {
		chunks.push_back(parseChunkFromRegion(regionPath, chunkX, chunkZ));
	}

	return chunks;
}

std::vector<std::pair<int32_t, int32_t>> World::DirectChunkParser::getAvailableChunks(const std::filesystem::path& regionPath) {
	// Stub implementation
	(void)regionPath;
	return {};
}
