#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <functional>
#include <string>
#include <vector>

class Player;
class Server;

// The commands a player types in chat. Registered once (Commands::registerCommands, called when the server
// starts), then run from the chat packets (handlePlayState). Game thread only.
//
// Adding a command: give it a name, a usage string, and what runs it — then register it in
// src/Commands.cpp's registerCommands. The run function gets the arguments, split on spaces, without the
// command's name ("tp 100 64 100" runs "tp" with {"100", "64", "100"}).
namespace Commands {
	// Commands.LEVEL_*: the op levels
	constexpr int CONSOLE_NEVER = -2, CONSOLE_ALWAYS = -1;
	constexpr int LEVEL_ALL = 0, LEVEL_MODERATORS = 1, LEVEL_GAMEMASTERS = 2, LEVEL_ADMINS = 3, LEVEL_OWNERS = 4;

	struct Command {
		std::string name;																	  // "tp"
		std::string usage;																	  // "tp [<player>] <x> <y> <z>"
		std::function<void(Server&, Player&, const std::vector<std::string>&)> run;			  // Empty: known but disabled
		int			permission = LEVEL_GAMEMASTERS; // The op level it needs (Player::permissionLevel)
		// From the server console: CONSOLE_NEVER (it needs a player: a position, itself as target), CONSOLE_ALWAYS, or
		// the index of the argument naming its target player (refused without it, or with @s / @p)
		int			console = CONSOLE_NEVER;
	};

	// Registers every built-in command (called once, when the server starts)
	void registerCommands();
	// Runs a command line without the leading '/', e.g. "tp 100 64 100". Shows an error if it is unknown
	void run(Server& server, Player& player, const std::string& line);

	// The command tree for the client's suggestions and syntax coloring (Commands packet), sent when it joins
	void sendCommandTree(Server& server, Player& player);
	// Command Suggestion Request: what to suggest for the arguments that ask the server (text: as typed, with its '/')
	void suggest(Server& server, Player& player, int transaction, const std::string& text);

	// A message in the player's own chat (System Chat, in the chat)
	void message(Server& server, Player& player, const std::string& text);
	// A player's line for everyone to see (chat.type.text: its name, then the message)
	void broadcastChat(Server& server, Player& player, const std::string& text);
	// A translated message in the player's own chat ("commands.op.success" and its arguments)
	void translated(Server& server, Player& player, const std::string& key, const std::vector<std::string>& args = {});
	// The same in red (a command's failure)
	void failure(Server& server, Player& player, const std::string& key, const std::vector<std::string>& args = {});
	// The same for every player in the game
	void broadcastTranslated(Server& server, const std::string& key, const std::vector<std::string>& args);
	// A translation key with its arguments as English text, for the console ("Made Bob a server operator"); keys it
	// doesn't know: the key and its arguments
	std::string englishText(const std::string& key, const std::vector<std::string>& args);
	// Whether the player may run this command (its op level)
	bool canUse(const Player& player, const Command& command);
	// Its usage, after a wrong call ("Usage: /<usage>")
	void usage(Server& server, Player& player, const std::string& usage);
} // namespace Commands

#endif