#ifndef LIGHT_HPP
#define LIGHT_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

class Chunk;
class GameData;

// Light level (0-15) of every block of a 16x16x16 section
struct SectionLight {
	std::vector<uint8_t> nibbles;	  // 2048 bytes, index (y << 8) | (z << 4) | x, low nibble first. Empty when uniform
	uint8_t				 uniform = 0; // Level of every block when nibbles is empty

	bool	isUniform() const { return nibbles.empty(); }
	uint8_t get(int index) const { return isUniform() ? uniform : (nibbles[index >> 1] >> ((index & 1) * 4)) & 0xF; }
};

// Sky and block light of a chunk's sections. Most sections are uniform (open sky, underground, no light source),
// so they cost one byte each
struct ChunkLight {
	std::vector<SectionLight> sky;
	std::vector<SectionLight> block;

	size_t memoryUsage() const;
};

// How each block state interacts with light, indexed by state id
struct LightTables {
	std::vector<uint8_t> absorption;   // Light lost when passing through, at least 1 is always lost
	std::vector<uint8_t> emission;	   // Torches 14, glowstone 15...
	std::vector<uint8_t> skylightDown; // 1 if sky light goes straight down through it without loss (air, glass)

	explicit LightTables(const GameData& gameData);
};

namespace LightEngine {
	// Sky and block light of the center chunk of a 3x3 area: area[(dz + 1) * 3 + (dx + 1)], area[4] being the
	// center. Light travels at most 15 blocks and a chunk is 16 wide, so the center's light only depends on these 9
	// chunks: with all of them, the result is exact. A missing neighbor (nullptr) is treated as opaque.
	// Vanilla rules: sky light is 15 down to the first block that stops it, then it and block light spread to the 6
	// neighbors, losing max(1, absorption of the block entered) per block.
	// Locks each chunk's mutex while reading its blocks.
	ChunkLight compute(const Chunk* const area[9], const LightTables& tables);

	// Sections whose light changed in each chunk of a 3x3 window
	struct Changes {
		std::vector<bool> sky[9];
		std::vector<bool> block[9];
	};

	// Updates the light after the block at (x, y, z) of the center chunk changed (x, z local, y from the bottom of
	// the world). Only the blocks whose light changes are visited: first the light that came through the old block is
	// removed, then light spreads again from what remains around it and from the block itself.
	// The effect stays within 15 blocks, so inside the 3x3 window. All 9 chunks must be lit, and the caller holds
	// their mutexes.
	Changes update(Chunk* const window[9], int x, int y, int z, const LightTables& tables);
} // namespace LightEngine

#endif
