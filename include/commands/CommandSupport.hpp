#ifndef COMMANDS_COMMANDSUPPORT_HPP
#define COMMANDS_COMMANDSUPPORT_HPP

#include "Commands.hpp"

#include <string>
#include <vector>

class Player;
class Server;

// Internal helpers shared by the commands in src/commands/. Not meant to be included outside of Commands.
namespace Commands {
	// The list of registered commands (defined in Commands.cpp)
	std::vector<Command>& commandList();

	// "~x" is relative to current, "x" is absolute
	bool parseCoord(double current, const std::string& arg, double& out);
	// The player is put at x, y, z on the game thread: its own client gets a teleport, the others its movement
	void teleport(Server& server, Player& player, double x, double y, double z);
	// "x y z" as integers, for chat messages
	std::string coords(double x, double y, double z);
	std::string coordinates(const Player& player);

	// One registration function per command, called by Commands::registerCommands
	void registerTp();
	void registerGamemode();
	void registerSpawn();
	void registerSetWorldSpawn();
	void registerSpawnPoint();
	void registerExecute();
	void registerFill();
	void registerHelp();
	void registerTime();
	void registerList();
	void registerSetBlock();
	void registerGive();
	void registerClear();
	void registerKill();
	void registerDifficulty();
	void registerWeather();
} // namespace Commands

#endif