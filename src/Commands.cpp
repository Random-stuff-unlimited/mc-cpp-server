#include "Commands.hpp"

#include "PacketIds.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/World.hpp"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace {
	std::vector<Commands::Command>& commands() {
		static std::vector<Commands::Command> list;
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

	std::vector<std::string> split(const std::string& line) {
		std::vector<std::string> parts;
		std::istringstream		  stream(line);
		std::string				  part;
		while (stream >> part) parts.push_back(part);
		return parts;
	}

	std::string coords(double x, double y, double z) {
		return std::to_string(static_cast<int>(x)) + " " + std::to_string(static_cast<int>(y)) + " " + std::to_string(static_cast<int>(z));
	}

	std::string coordinates(const Player& player) { return coords(player.getX(), player.getY(), player.getZ()); }
} // namespace

namespace Commands {
	void registerCommands() {
		if (!commands().empty()) return;

		commands().push_back({// Teleports the player (or another) to absolute or relative (with ~) coordinates
							  "tp", "tp [<player>] <x> <y> <z>", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  Player* target = &player;
								  size_t	start = 0;
								  if (args.size() == 4) {
									  auto found = server.findPlayersByName(args[0]);
									  if (found.empty()) {
										  message(server, player, "Player not found: " + args[0]);
										  return;
									  }
									  target = found[0].get();
									  start	 = 1;
								  }
								  if (args.size() != start + 3) {
									  usage(server, player, "tp [<player>] <x> <y> <z>");
									  return;
								  }
								  double x, y, z;
								  if (!parseCoord(target->getX(), args[start], x) || !parseCoord(target->getY(), args[start + 1], y) ||
									  !parseCoord(target->getZ(), args[start + 2], z)) {
									  usage(server, player, "tp [<player>] <x> <y> <z>");
									  return;
								  }
								  teleport(server, *target, x, y, z);
								  message(server, player,
										  (target == &player ? "Teleported to " : "Teleported " + target->getPlayerName() + " to ") + coordinates(*target));
							  }});

		commands().push_back({// Teleports to the world spawn
							  "spawn", "spawn", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (!args.empty()) {
									  usage(server, player, "spawn");
									  return;
								  }
								  const World::Spawn& spawn = server.getWorld().getSpawn();
								  teleport(server, player, spawn.x, spawn.y, spawn.z);
								  message(server, player, "Teleported to the world spawn");
							  }});

		commands().push_back({// Moves the world spawn to the player, or to the coordinates given
							  "setworldspawn", "setworldspawn [<x> <y> <z>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  double x = player.getX(), y = player.getY(), z = player.getZ();
								  if (args.size() == 3) {
									  if (!parseCoord(player.getX(), args[0], x) || !parseCoord(player.getY(), args[1], y) ||
										  !parseCoord(player.getZ(), args[2], z)) {
										  usage(server, player, "setworldspawn [<x> <y> <z>]");
										  return;
									  }
								  } else if (!args.empty()) {
									  usage(server, player, "setworldspawn [<x> <y> <z>]");
									  return;
								  }
								  server.getWorld().setSpawn(x, y, z);
								  message(server, player, "World spawn set to " + coords(x, y, z));
							  }});

		commands().push_back({// Sets the player's own respawn point (their bed), at their position or the coordinates given
							  "spawnpoint", "spawnpoint [<x> <y> <z>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  double x = player.getX(), y = player.getY(), z = player.getZ();
								  if (args.size() == 3) {
									  if (!parseCoord(player.getX(), args[0], x) || !parseCoord(player.getY(), args[1], y) ||
										  !parseCoord(player.getZ(), args[2], z)) {
										  usage(server, player, "spawnpoint [<x> <y> <z>]");
										  return;
									  }
								  } else if (!args.empty()) {
									  usage(server, player, "spawnpoint [<x> <y> <z>]");
									  return;
								  }
								  player.spawn() = {true, static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)),
													static_cast<int>(std::floor(z)), server.getWorld().getDimensionName(), true};
								  message(server, player, "Respawn point set to " + coordinates(player));
							  }});

		commands().push_back({"help", "help [<command>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (!args.empty()) {
									  for (const Command& command : commands()) {
										  if (command.name == args[0]) {
											  message(server, player, "/" + command.usage);
											  return;
										  }
									  }
									  message(server, player, "Unknown command: " + args[0]);
									  return;
								  }
								  std::string list;
								  for (const Command& command : commands()) {
									  if (!list.empty()) list += ", ";
									  list += "/" + command.name;
								  }
								  message(server, player, "Commands: " + list);
							  }});
	}

	void run(Server& server, Player& player, const std::string& line) {
		size_t space = line.find(' ');
		std::string name = space == std::string::npos ? line : line.substr(0, space);
		if (name.empty()) return;
		std::vector<std::string> args = space == std::string::npos ? std::vector<std::string>() : split(line.substr(space + 1));
		for (const Command& command : commands()) {
			if (command.name != name) continue;
			if (command.run) command.run(server, player, args);
			return;
		}
		// Unknown or incomplete command
		Buffer error;
		TextComponent::writeTranslatable(error, "commands.unknown.command", {});
		error.writeBool(false);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, error, server);
	}

	void message(Server& server, Player& player, const std::string& text) {
		Buffer message;
		TextComponent::writeText(message, text);
		message.writeBool(false); // In the chat, not above the hotbar
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SYSTEM_CHAT, message, server);
	}

	void broadcastChat(Server& server, Player& player, const std::string& text) {
		Buffer message;
		TextComponent::writeTranslatable(message, "chat.type.text", {player.getPlayerName(), text});
		message.writeBool(false);
		std::vector<uint8_t> frame =
				Packet::buildFrame(PacketId::Play::Clientbound::SYSTEM_CHAT, message.getData(), server.getConfig().getCompressionThreshold());
		for (const auto& other : server.getGamePlayers()) Packet::sendFrame(other, frame, server);
	}

	void usage(Server& server, Player& player, const std::string& usage) {
		message(server, player, "Usage: /" + usage);
	}
} // namespace Commands