#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP

// ==================================================================================
//  Adding a world generator
//
//  1. Create src/world/generators/MyGenerator.hpp with a class deriving from WorldGenerator:
//       - its constructor reads its options from GeneratorSettings (level.json)
//       - generate() fills one chunk through ChunkBuilder
//       - spawnHeight() tells where players appear in a new world
//  2. In src/world/generators/generators.cpp: #include it and add one line to the GENERATORS table
//  3. Select it in <world>/level.json: "generator": { "type": "my_generator", ...options }
//
//  FlatGenerator.hpp is a complete example, VoidGenerator.hpp the smallest one.
// ==================================================================================

#include "lib/json.hpp"

#include <cstdint>
#include <memory>
#include <string>

class Chunk;
class GameData;

// What a generator receives when the world loads
class GeneratorSettings {
  public:
	GeneratorSettings(const nlohmann::json& options, const GameData& gameData, int minY, int height, int64_t seed);

	const nlohmann::json& options; // The "generator" object of level.json
	const int			  minY;	   // Lowest block of the world
	const int			  height;  // Number of blocks from minY
	const int64_t		  seed;

	// Resolve names once in the constructor, then use the ids in generate(). Throw on unknown names
	int block(const std::string& name) const; // "minecraft:stone" or "minecraft:oak_log[axis=x]"
	int biome(const std::string& name) const; // "minecraft:plains"

  private:
	const GameData& _gameData;
};

// What generate() writes to. x and z are local to the chunk (0-15), y is the world height
class ChunkBuilder {
  public:
	explicit ChunkBuilder(Chunk& chunk) : _chunk(chunk) {}

	int chunkX() const;
	int chunkZ() const;
	int minY() const;
	int maxY() const; // Exclusive

	// World coordinates of a local position: worldX(x) = chunkX() * 16 + x
	int worldX(int x) const { return chunkX() * 16 + x; }
	int worldZ(int z) const { return chunkZ() * 16 + z; }

	void setBlock(int x, int y, int z, int block);
	void fillLayer(int y, int block); // The whole 16x16 layer at height y
	void setBiome(int x, int y, int z, int biome);
	void fillBiome(int biome); // The whole chunk

  private:
	Chunk& _chunk;
};

class WorldGenerator {
  public:
	virtual ~WorldGenerator() = default;

	// Called on the world I/O threads, possibly for several chunks at the same time: don't modify members here
	virtual void generate(ChunkBuilder& chunk) const = 0;

	// Height where players spawn in a new world
	virtual int spawnHeight() const = 0;
};

// Creates the generator named by settings.options["type"]. Throws if unknown
std::unique_ptr<WorldGenerator> createGenerator(const GeneratorSettings& settings);

#endif
