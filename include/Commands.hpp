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
	struct Command {
		std::string name;																	  // "tp"
		std::string usage;																	  // "tp [<player>] <x> <y> <z>"
		std::function<void(Server&, Player&, const std::vector<std::string>&)> run;			  // Empty: known but disabled
	};

	// Registers every built-in command (called once, when the server starts)
	void registerCommands();
	// Runs a command line without the leading '/', e.g. "tp 100 64 100". Shows an error if it is unknown
	void run(Server& server, Player& player, const std::string& line);

	// A message in the player's own chat (System Chat, in the chat)
	void message(Server& server, Player& player, const std::string& text);
	// A player's line for everyone to see (chat.type.text: its name, then the message)
	void broadcastChat(Server& server, Player& player, const std::string& text);
	// Its usage, after a wrong call ("Usage: /<usage>")
	void usage(Server& server, Player& player, const std::string& usage);
} // namespace Commands

#endif