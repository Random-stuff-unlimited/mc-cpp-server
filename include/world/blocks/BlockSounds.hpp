#ifndef BLOCK_SOUNDS_HPP
#define BLOCK_SOUNDS_HPP

#include <string>

class GameData;

// SoundType: the sounds of a block (break, step, place, hit, fall). The game data has no sound types: they are found
// from the block's name (its material: planks are wood, sand is sand...), stone for the others
namespace BlockSounds {
	// "minecraft:block.<type>.<kind>", kind one of break, step, place, hit, fall
	std::string sound(const GameData& gameData, int block, const char* kind);
} // namespace BlockSounds

#endif
