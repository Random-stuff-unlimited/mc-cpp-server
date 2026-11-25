#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP

#include <array>
#include <filesystem>
#include <map>
#include <set>
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

	// PlayerTeam: its members (player names), its color, prefix and suffix, friendly fire, and how its members' names
	// and collisions show to others
	struct Team {
		std::string				 name, displayName, prefix, suffix;
		int						 color			  = 21; // ChatFormatting id: 21 is reset (none)
		bool					 friendlyFire	  = true, seeFriendlyInvisibles = true;
		std::string				 nametagVisibility = "always", collisionRule = "always";
		std::vector<std::string> members;
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
	// Score.isLocked: a trigger objective's score a player may change once with /trigger (enable unlocks it)
	void						   enableTrigger(const std::string& owner, const std::string& objective) { _unlocked.insert(owner + "\n" + objective); }
	bool						   useTrigger(const std::string& owner, const std::string& objective) { return _unlocked.erase(owner + "\n" + objective) > 0; }
	std::vector<std::string>	   owners() const;
	std::map<std::string, int>	   scoresOf(const std::string& owner) const;

	const Team*			  team(const std::string& name) const;
	std::vector<const Team*> teams() const;
	// The team of a player name (or any score holder), null if none
	const Team*			  teamOf(const std::string& member) const;
	bool				  addTeam(const std::string& name, const std::string& displayName);
	bool				  removeTeam(const std::string& name);
	// Joins a team (leaving its old one); false if it was already in it
	bool				  join(const std::string& team, const std::string& member);
	bool				  leave(const std::string& member);
	// Changes a team's options ("color", "prefix", "friendlyFire"...); false if the value isn't valid
	bool				  modifyTeam(const std::string& name, const std::string& option, const std::string& value);
	// Whether one may hurt the other (PlayerTeam.isAllowFriendlyFire): false only in the same team without friendly fire
	bool				  canHurt(const std::string& attacker, const std::string& victim) const;
	// EntitySelector.pushableBy: the team collision rules let the pusher push the other (members by name: a player's
	// name, an entity's UUID)
	bool				  canPush(const std::string& pusher, const std::string& pushed) const;
	bool				  hasTeams() const { return !_teams.empty(); }
	// ChatFormatting colors by name ("red"... "reset"), -1 if unknown
	static int			  colorByName(const std::string& name);

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
	std::map<std::string, Team>						   _teams;
	std::set<std::string>							   _unlocked; // "owner\nobjective" of the enabled triggers

	// ClientboundSetPlayerTeamPacket: 0 add, 1 remove, 2 change, 3 join, 4 leave
	void sendTeam(const Team& team, int method, const std::vector<std::string>& members, Player* to = nullptr);

	void addToCriterion(const std::string& criterion, const std::string& owner, int amount);
	void sendObjective(const Objective& objective, int mode, Player* to = nullptr);
	void sendScore(const std::string& owner, const std::string& objective, int value);
	void sendReset(const std::string& owner, const std::string* objective);
	void sendDisplay(int slot, Player* to = nullptr);
	void broadcast(int packetId, class Buffer& data);
};

#endif
