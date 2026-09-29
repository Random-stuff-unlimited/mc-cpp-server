#ifndef XP_HPP
#define XP_HPP

class GameData;
class Player;
class Server;

// The player's experience (vanilla's XpBar): levels, the progress toward the next and the total, how much a block
// or a mob is worth, and the SET_EXPERIENCE packet. Game thread only.
namespace Xp {
	// The points needed to go from `level` to `level + 1` (XpBar.getXpNeededForNextLevel)
	int xpNeededForNextLevel(int level);
	// The total points accumulated at the start of `level` (XpBar.getTotalExperienceForLevel)
	int xpToLevel(int level);

	// Player.giveExperiencePoints: adds points, leveling up as needed (never below 0)
	void addExperience(Server& server, Player& player, int amount);
	// Player.giveExperienceLevels: adds levels, keeping the fraction of the progress bar
	void addLevels(Server& server, Player& player, int levels);
	// SET_EXPERIENCE to the player (its level, progress and total)
	void send(Server& server, Player& player);

	// The XP an orb dropped by mining this block gives, 0 if the block drops none
	int blockXp(const GameData& gameData, int blockId);
	// The XP this entity type drops when killed, 0 if none
	int mobXp(const GameData& gameData, int typeId);
} // namespace Xp

#endif