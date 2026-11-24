#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/PacketIds.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "logger.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"

#include <cstdlib>
#include <sstream>

namespace {
	// Splits a command line into its arguments, on spaces
	std::vector<std::string> split(const std::string& line) {
		std::vector<std::string> parts;
		std::istringstream		  stream(line);
		std::string				  part;
		while (stream >> part) parts.push_back(part);
		return parts;
	}
} // namespace

namespace Commands {
	std::vector<Command>& commandList() {
		static std::vector<Command> list;
		return list;
	}

	// "~x" is relative to current, "x" is absolute
	bool parseCoord(double current, const std::string& arg, double& out) {
		std::string s = arg;
		if (!s.empty() && s[0] == '~') {
			s.erase(0, 1);
			if (s.empty()) {
				out = current;
				return true;
			}
		} else {
			current = 0;
		}
		char* end = nullptr;
		double value = std::strtod(s.c_str(), &end);
		if (end == s.c_str() || *end != '\0') return false;
		out = current + value;
		return true;
	}

	// The player is put at x, y, z on the game thread: its own client gets a teleport, the others its movement
	void teleport(Server& server, Player& player, double x, double y, double z) {
		player.setPosition(x, y, z);
		player.setOnGround(true);
		player.combat().fallDistance = 0; // No fall damage for the distance flown over
		Packet packet(player.shared_from_this());
		synchronizePlayerPositionPacket(packet, server);
		if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->onPlayerMove(x, z);
		server.getPlayerTracker().move(&player);
	}

	std::string coords(double x, double y, double z) {
		return std::to_string(static_cast<int>(x)) + " " + std::to_string(static_cast<int>(y)) + " " + std::to_string(static_cast<int>(z));
	}

	std::vector<Player*> playerTargets(Server& server, Player& self, const std::string& selector) {
		std::vector<Player*> found;
		if (selector == "@s" || selector == "@p") {
			if (!self.isConsole()) found.push_back(&self);
		} else if (selector == "@a") {
			for (const auto& player : server.getGamePlayers()) {
				if (!player->isDisconnected()) found.push_back(player.get());
			}
		} else {
			for (const auto& player : server.findPlayersByName(selector)) found.push_back(player.get());
		}
		return found;
	}

	std::string coordinates(const Player& player) { return coords(player.getX(), player.getY(), player.getZ()); }

	void registerCommands() {
		if (!commandList().empty()) return;

		registerTp();
		registerGamemode();
		registerSpawn();
		registerSetWorldSpawn();
		registerSpawnPoint();
		registerExecute();
		registerFill();
		registerTime();
		registerList();
		registerSetBlock();
		registerGive();
		registerClear();
		registerKill();
		registerDifficulty();
		registerWeather();
		registerEffect();
		registerSummon();
		registerExperience();
		registerEnchant();
		registerAdmin();
		registerChat();
		registerHelp();
		registerServer();
		registerTitle();
		// The commands everyone may run (the others need an op level of 2, the admin ones set theirs)
		for (Command& command : commandList()) {
			if (command.name == "help" || command.name == "list" || command.name == "spawn") command.permission = LEVEL_ALL;
		}
		// What the console may run: the commands that don't need a player, and those naming their target
		static const std::vector<std::pair<std::string, int>> CONSOLE = {
			{"help", CONSOLE_ALWAYS},	 {"list", CONSOLE_ALWAYS},		 {"op", CONSOLE_ALWAYS},	   {"deop", CONSOLE_ALWAYS},
			{"kick", CONSOLE_ALWAYS},	 {"ban", CONSOLE_ALWAYS},		 {"ban-ip", CONSOLE_ALWAYS},   {"pardon", CONSOLE_ALWAYS},
			{"pardon-ip", CONSOLE_ALWAYS}, {"banlist", CONSOLE_ALWAYS},	 {"whitelist", CONSOLE_ALWAYS}, {"say", CONSOLE_ALWAYS},
			{"tell", CONSOLE_ALWAYS},	 {"msg", CONSOLE_ALWAYS},		 {"w", CONSOLE_ALWAYS},		   {"me", CONSOLE_ALWAYS},
			{"time", CONSOLE_ALWAYS},	 {"weather", CONSOLE_ALWAYS},	 {"difficulty", CONSOLE_ALWAYS}, {"stop", CONSOLE_ALWAYS},
			{"save-all", CONSOLE_ALWAYS}, {"xp", CONSOLE_ALWAYS},		 {"experience", CONSOLE_ALWAYS}, {"enchant", CONSOLE_ALWAYS},
			{"give", 0},				 {"clear", 0},					 {"kill", 0},				   {"effect", 1},
			{"gamemode", 1},			 {"title", 0},			 {"gm", 1},
		};
		for (Command& command : commandList()) {
			for (const auto& [name, console] : CONSOLE) {
				if (command.name == name) command.console = console;
			}
		}
	}

	bool canUse(const Player& player, const Command& command) { return command.run && player.hasPermissions(command.permission); }

	void run(Server& server, Player& player, const std::string& line) {
		size_t space = line.find(' ');
		std::string name = space == std::string::npos ? line : line.substr(0, space);
		if (name.empty()) return;
		std::vector<std::string> args = space == std::string::npos ? std::vector<std::string>() : split(line.substr(space + 1));
		for (const Command& command : commandList()) {
			if (command.name != name) continue;
			// Without the level, the command doesn't exist for the player (it isn't in its command tree either)
			if (!canUse(player, command)) break;
			if (player.isConsole() && command.console != CONSOLE_ALWAYS) {
				const std::string* target = command.console >= 0 && static_cast<size_t>(command.console) < args.size() ? &args[command.console] : nullptr;
				if (!target || *target == "@s" || *target == "@p") return failure(server, player, "permissions.requires.player");
			}
			command.run(server, player, args);
			return;
		}
		// Unknown or incomplete command
		if (player.isConsole()) return g_logger->logGameInfo(WARN, "Unknown command: " + name + " (\"help\" lists them)", "Console");
		Buffer error;
		TextComponent::writeTranslatable(error, "commands.unknown.command", {});
		error.writeBool(false);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, error, server);
	}

	void message(Server& server, Player& player, const std::string& text) {
		if (player.isConsole()) return g_logger->logGameInfo(INFO, text, "Console");
		Buffer message;
		TextComponent::writeText(message, text);
		message.writeBool(false); // In the chat, not above the hotbar
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, message, server);
	}

	void broadcastChat(Server& server, Player& player, const std::string& text) {
		g_logger->logGameInfo(INFO, englishText("chat.type.text", {player.getPlayerName(), text}), "Chat");
		Buffer message;
		TextComponent::writeTranslatable(message, "chat.type.text", {player.getPlayerName(), text});
		message.writeBool(false);
		std::vector<uint8_t> frame =
				Packet::buildFrame(PacketId::Play::Clientbound::SYSTEM_CHAT, message.getData(), server.getConfig().getCompressionThreshold());
		for (const auto& other : server.getGamePlayers()) Packet::sendFrame(other, frame, server);
	}

	void translated(Server& server, Player& player, const std::string& key, const std::vector<std::string>& args) {
		if (player.isConsole()) return g_logger->logGameInfo(INFO, englishText(key, args), "Console");
		Buffer message;
		TextComponent::writeTranslatable(message, key, args);
		message.writeBool(false);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, message, server);
	}

	void failure(Server& server, Player& player, const std::string& key, const std::vector<std::string>& args) {
		if (player.isConsole()) return g_logger->logGameInfo(WARN, englishText(key, args), "Console");
		Buffer message;
		TextComponent::writeTranslatable(message, key, args, "red");
		message.writeBool(false);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, message, server);
	}

	void broadcastTranslated(Server& server, const std::string& key, const std::vector<std::string>& args) {
		g_logger->logGameInfo(INFO, englishText(key, args), "Chat"); // MinecraftServer logs what it broadcasts
		Buffer message;
		TextComponent::writeTranslatable(message, key, args);
		message.writeBool(false);
		std::vector<uint8_t> frame =
				Packet::buildFrame(PacketId::Play::Clientbound::SYSTEM_CHAT, message.getData(), server.getConfig().getCompressionThreshold());
		for (const auto& other : server.getGamePlayers()) Packet::sendFrame(other, frame, server);
	}

	void usage(Server& server, Player& player, const std::string& usage) {
		message(server, player, "Usage: /" + usage);
	}
} // namespace Commands