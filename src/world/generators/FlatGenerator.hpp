#ifndef FLAT_GENERATOR_HPP
#define FLAT_GENERATOR_HPP

#include "world/WorldGenerator.hpp"

#include <vector>

// Superflat: layers of blocks stacked from the bottom of the world.
//
// level.json:
//   "generator": {
//     "type": "flat",
//     "biome": "minecraft:plains",
//     "layers": [
//       { "block": "minecraft:bedrock", "height": 1 },
//       { "block": "minecraft:dirt", "height": 2 },
//       { "block": "minecraft:grass_block", "height": 1 }
//     ]
//   }
class FlatGenerator : public WorldGenerator {
  public:
	explicit FlatGenerator(const GeneratorSettings& settings) : _minY(settings.minY) {
		_biome = settings.biome(settings.options.value("biome", "minecraft:plains"));
		for (const auto& layer : settings.options.at("layers")) {
			_layers.push_back({settings.block(layer.at("block").get<std::string>()), layer.value("height", 1)});
		}
	}

	void generate(ChunkBuilder& chunk) const override {
		int y = chunk.minY();
		for (const Layer& layer : _layers) {
			for (int i = 0; i < layer.height; i++) chunk.fillLayer(y++, layer.block);
		}
		chunk.fillBiome(_biome);
	}

	int spawnHeight() const override {
		int height = _minY;
		for (const Layer& layer : _layers) height += layer.height;
		return height;
	}

  private:
	struct Layer {
		int block;
		int height;
	};

	int				   _minY;
	int				   _biome;
	std::vector<Layer> _layers;
};

#endif
