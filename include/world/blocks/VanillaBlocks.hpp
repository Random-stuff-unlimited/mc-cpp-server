#ifndef VANILLA_BLOCKS_HPP
#define VANILLA_BLOCKS_HPP

class GameData;
class Level;

// Gives every block the behavior of its vanilla class (LiquidBlock, LadderBlock...), found through the "classes" of
// blocks.json. Blocks whose class isn't ported yet keep the default behavior (nothing happens)
void registerVanillaBlocks(Level& level, const GameData& gameData);

#endif
