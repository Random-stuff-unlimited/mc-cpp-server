#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

class Player;
class Server;

// ServerScoreboard: objectives (a criterion, a display name, integer or hearts), the scores of their holders (player
// names, or any name), and what the display slots show (the tab list, the sidebar, below the names). The players see
// it all and every change. The automatic criteria follow the game: deaths, kills, health, food, level. Saved in
// <world>/scoreboard.json. Teams aren't here
class Scoreboard {
  public:
	// DisplaySlot: list, sidebar, below_name, then the 16 team colors' sidebars
	static constexpr int SLOT_COUNT = 19;
	struct Objective {
		std::string name, criterion, displayName;
		bool		hearts = false; // ObjectiveCriteria.RenderType
	};

	explicit Scoreboard(Server& server) : _server(server) {}
	void load(const std::filesystem::path& file);
	void save() const;

	// ObjectiveCriteria: dummy, trigger and the automatic ones (deathCount, playerKillCount, totalKillCount, health,
	// food, level, xp, air, armor)
	static bool					   isCriterion(const std::string& name);
	static bool					   isReadOnly(const std::string& criterion);
	// DisplaySlot.byName: "list", "sidebar", "below_name", "sidebar.team.red"... -1 if none
	static int					   slotByName(const std::string& name);
	static std::string			   slotName(int slot);

	const Objective*			   objective(const std::string& name) const;
	std::vector<const Objective*>  objectives() const;
	bool						   addObjective(const std::string& name, const std::string& criterion, const std::string& displayName);
	bool						   removeObjective(const std::string& name);
	void						   setDisplayName(const std::string& name, const std::string& displayName);
	void						   setHearts(const std::string& name, bool hearts);
	void						   setDisplay(int slot, const std::string& objective); // "" clears it
	const std::string&			   display(int slot) const { return _display[slot]; }

	std::optional<int>			   score(const std::string& owner, const std::string& objective) const;
	void						   setScore(const std::string& owner, const std::string& objective, int value);
	int							   addScore(const std::string& owner, const std::string& objective, int amount); // The new value
	bool						   resetScore(const std::string& owner, const std::string& objective);
	bool						   resetScores(const std::string& owner);
	std::vector<std::string>	   owners() const;
	std::map<std::string, int>	   scoresOf(const std::string& owner) const;

	// The criteria the game counts
	void onPlayerDeath(Player& victim, Player* killer);
	void onKill(Player& killer);
	// health, food, level, xp, air, armor: the players' values when they changed (each tick)
	void tick();
	// Everything, to a player joining
	void sendAll(Player& player);

  private:
	Server&											   _server;
	std::filesystem::path							   _file;
	std::map<std::string, Objective>				   _objectives;
	std::map<std::string, std::map<std::string, int>>  _scores; // Owner -> objective -> value
	std::array<std::string, SLOT_COUNT>				   _display;

	void addToCriterion(const std::string& criterion, const std::string& owner, int amount);
	void sendObjective(const Objective& objective, int mode, Player* to = nullptr);
	void sendScore(const std::string& owner, const std::string& objective, int value);
	void sendReset(const std::string& owner, const std::string* objective);
	void sendDisplay(int slot, Player* to = nullptr);
	void broadcast(int packetId, class Buffer& data);
};

#endif
