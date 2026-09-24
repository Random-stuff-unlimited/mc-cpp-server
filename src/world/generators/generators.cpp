#include "data/GameData.hpp"
#include "world/Chunk.hpp"
#include "world/WorldGenerator.hpp"

#include <functional>
#include <map>
#include <stdexcept>

#include "FlatGenerator.hpp"
#include "VoidGenerator.hpp"

namespace {
	using GeneratorFactory = std::function<std::unique_ptr<WorldGenerator>(const GeneratorSettings&)>;

	template <typename T> std::unique_ptr<WorldGenerator> create(const GeneratorSettings& settings) { return std::make_unique<T>(settings); }

	// ===== Available generators: "type" in level.json -> class =====
	const std::map<std::string, GeneratorFactory> GENERATORS = {
			{"flat", create<FlatGenerator>},
			{"void", create<VoidGenerator>},
	};
} // namespace

std::unique_ptr<WorldGenerator> createGenerator(const GeneratorSettings& settings) {
	std::string type = settings.options.value("type", "");
	auto		it	 = GENERATORS.find(type);
	if (it == GENERATORS.end()) throw std::runtime_error("Unknown world generator \"" + type + "\" in level.json");
	return it->second(settings);
}

// ===== GeneratorSettings =====

GeneratorSettings::GeneratorSettings(const nlohmann::json& options, const GameData& gameData, int minY, int height, int64_t seed)
	: options(options), minY(minY), height(height), seed(seed), _gameData(gameData) {}

int GeneratorSettings::block(const std::string& name) const {
	int id = _gameData.getBlockStateFromName(name);
	if (id < 0) id = _gameData.getDefaultBlockState(name);
	if (id < 0) throw std::runtime_error("Unknown block \"" + name + "\" in the generator settings");
	return id;
}

int GeneratorSettings::biome(const std::string& name) const {
	int id = _gameData.getSyncedId("minecraft:worldgen/biome", name);
	if (id < 0) throw std::runtime_error("Unknown biome \"" + name + "\" in the generator settings");
	return id;
}

// ===== ChunkBuilder =====

int ChunkBuilder::chunkX() const { return _chunk.x(); }
int ChunkBuilder::chunkZ() const { return _chunk.z(); }
int ChunkBuilder::minY() const { return _chunk.minY(); }
int ChunkBuilder::maxY() const { return _chunk.maxY(); }

void ChunkBuilder::setBlock(int x, int y, int z, int block) {
	if (x < 0 || x > 15 || z < 0 || z > 15 || y < minY() || y >= maxY()) return;
	_chunk.setBlock(x, y, z, static_cast<uint32_t>(block));
}

void ChunkBuilder::fillLayer(int y, int block) {
	if (y < minY() || y >= maxY()) return;
	for (int z = 0; z < 16; z++) {
		for (int x = 0; x < 16; x++) _chunk.setBlock(x, y, z, static_cast<uint32_t>(block));
	}
}

void ChunkBuilder::setBiome(int x, int y, int z, int biome) {
	if (x < 0 || x > 15 || z < 0 || z > 15 || y < minY() || y >= maxY()) return;
	_chunk.setBiome(x, y, z, static_cast<uint32_t>(biome));
}

void ChunkBuilder::fillBiome(int biome) {
	for (ChunkSection& section : _chunk.sections()) section.biomes.fill(static_cast<uint32_t>(biome));
	_chunk.markModified();
}
