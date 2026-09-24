#ifndef VOID_GENERATOR_HPP
#define VOID_GENERATOR_HPP

#include "world/WorldGenerator.hpp"

// Empty world (air only), for maps built by hand.
//
// level.json:
//   "generator": { "type": "void", "biome": "minecraft:the_void" }
class VoidGenerator : public WorldGenerator {
  public:
	explicit VoidGenerator(const GeneratorSettings& settings) { _biome = settings.biome(settings.options.value("biome", "minecraft:the_void")); }

	void generate(ChunkBuilder& chunk) const override { chunk.fillBiome(_biome); }

	int spawnHeight() const override { return 64; }

  private:
	int _biome;
};

#endif
