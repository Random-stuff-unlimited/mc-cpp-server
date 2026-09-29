#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/PacketIds.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
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
		registerHelp();
	}

	void run(Server& server, Player& player, const std::string& line) {
		size_t space = line.find(' ');
		std::string name = space == std::string::npos ? line : line.substr(0, space);
		if (name.empty()) return;
		std::vector<std::string> args = space == std::string::npos ? std::vector<std::string>() : split(line.substr(space + 1));
		for (const Command& command : commandList()) {
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