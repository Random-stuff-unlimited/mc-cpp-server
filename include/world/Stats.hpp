#ifndef STATS_HPP
#define STATS_HPP

#include <filesystem>
#include <map>
#include <string>
#include <utility>

class GameData;
class Player;
class Server;

// ServerStatsCounter: a player's statistics (vanilla's Stats): per stat type (mined, crafted, used, broken, picked_up,
// dropped, killed, killed_by, custom) and per thing (block, item, entity type, custom stat), a count. Saved in
// <world>/stats/<uuid>.json in vanilla's layout; sent when the client opens its statistics screen
class PlayerStats {
  public:
	enum Type { Mined = 0, Crafted, Used, Broken, PickedUp, Dropped, Killed, KilledBy, Custom }; // minecraft:stat_type order

	int	 get(Type type, int id) const;
	void award(Type type, int id, int amount);
	void set(Type type, int id, int value);
	const std::map<std::pair<int, int>, int>& values() const { return _values; }

	// The registry of each type's ids (minecraft:block, minecraft:item, minecraft:entity_type, minecraft:custom_stat)
	static const char* registryOf(Type type);
	void load(const std::filesystem::path& file, const GameData& gameData);
	// The JSON text of the file
	std::string toJson(const GameData& gameData) const;

  private:
	std::map<std::pair<int, int>, int> _values;
};

namespace Stats {
	// A custom stat by name ("minecraft:jump"): awarded to the player
	void custom(Player& player, const GameData& gameData, const char* name, int amount = 1);
	// ServerStatsCounter.sendStats: Award Statistics, every stat the player has
	void send(Server& server, Player& player);
} // namespace Stats

#endif
