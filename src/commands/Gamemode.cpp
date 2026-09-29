#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"

namespace Commands {
	namespace {
		// The names, single letters and ids vanilla accepts for a game mode
		bool parseGameMode(const std::string& arg, GameMode& out) {
			if (arg == "survival" || arg == "s" || arg == "0") {
				out = GameMode::Survival;
				return true;
			}
			if (arg == "creative" || arg == "c" || arg == "1") {
				out = GameMode::Creative;
				return true;
			}
			if (arg == "adventure" || arg == "a" || arg == "2") {
				out = GameMode::Adventure;
				return true;
			}
			if (arg == "spectator" || arg == "sp" || arg == "3") {
				out = GameMode::Spectator;
				return true;
			}
			return false;
		}

		std::string gameModeName(GameMode mode) {
			switch (mode) {
			case GameMode::Survival: return "Survival";
			case GameMode::Creative: return "Creative";
			case GameMode::Adventure: return "Adventure";
			case GameMode::Spectator: return "Spectator";
			}
			return "Survival";
		}

		// "/gamemode <mode> [<player>]", and its alias "/gm": the mode first, or the player first like vanilla
		void runGamemode(Server& server, Player& player, const std::vector<std::string>& args) {
			if (args.empty() || args.size() > 2) {
				usage(server, player, "gamemode <mode> [<player>]");
				return;
			}

			GameMode mode;
			Player*	 target = &player;
			size_t	 index  = 0;
			if (parseGameMode(args[0], mode)) {
				index = 1;
			} else if (args.size() == 2 && parseGameMode(args[1], mode)) {
				auto found = server.findPlayersByName(args[0]);
				if (found.empty()) {
					message(server, player, "Player not found: " + args[0]);
					return;
				}
				target = found[0].get();
				index  = 2;
			} else {
				message(server, player, "Unknown game mode: " + args[0]);
				return;
			}
			if (index < args.size()) {
				auto found = server.findPlayersByName(args[index]);
				if (found.empty()) {
					message(server, player, "Player not found: " + args[index]);
					return;
				}
				target = found[0].get();
			}

			GameMode old = target->getGameMode();
			if (old == mode) {
				message(server, player, target->getPlayerName() + " is already in " + gameModeName(mode));
				return;
			}
			target->setPreviousGameMode(static_cast<int>(old));
			target->setGameMode(mode);
			// Only spectator flies on its own; leaving a flying mode stops it (survival lets gravity take over)
			if (mode == GameMode::Spectator) target->survival().flying = true;
			if (mode == GameMode::Survival || mode == GameMode::Adventure) target->survival().flying = false;

			// To the player: its abilities (fly, instant break...), then the "game mode changed" event
			Packet packet(target->shared_from_this());
			playerAbilitiesPacket(packet, server);
			Buffer event;
			event.writeUByte(3); // ClientboundGameEventPacket.CHANGE_GAME_MODE
			event.writeFloat(static_cast<float>(mode));
			Packet::send(target->shared_from_this(), PacketId::Play::Clientbound::GAME_EVENT, event, server);

			// Tab list: every player sees the new game mode
			Buffer info;
			info.writeUByte(1 << 2); // PlayerInfoUpdate.Action.UPDATE_GAME_MODE
			info.writeVarInt(1);
			info.writeUUID(target->getUUID());
			info.writeVarInt(static_cast<int32_t>(mode));
			std::vector<uint8_t> frame =
					Packet::buildFrame(PacketId::Play::Clientbound::PLAYER_INFO_UPDATE, info.getData(), server.getConfig().getCompressionThreshold());
			for (const auto& other : server.getGamePlayers()) Packet::sendFrame(other, frame, server);

			message(server, player,
					(target == &player ? "Set your game mode to " : "Set " + target->getPlayerName() + "'s game mode to ") + gameModeName(mode));
		}
	} // namespace

	void registerGamemode() {
		commandList().push_back({"gamemode", "gamemode <mode> [<player>]", runGamemode});
		commandList().push_back({"gm", "gm <mode> [<player>]", runGamemode});
	}
} // namespace Commands