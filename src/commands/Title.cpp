#include "Commands.hpp"

#include "PacketIds.hpp"
#include "commands/CommandSupport.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

// TitleCommand: /title <targets> (clear|reset|title <text>|subtitle <text>|actionbar <text>|times <in> <stay> <out>).
// Text is plain, or a JSON text component ({"text": "Hi", "color": "gold"})
namespace Commands {
	namespace {
		const std::string SYNTAX = "title <targets> (clear|reset|title <text>|subtitle <text>|actionbar <text>|times <fadeIn> <stay> <fadeOut>)";

		// "...single" with the player's name for one target, "...multiple" with the count for several
		void feedback(Server& server, Player& source, const std::string& key, const std::vector<Player*>& targets) {
			if (targets.size() == 1) {
				translated(server, source, key + ".single", {targets[0]->getPlayerName()});
			} else {
				translated(server, source, key + ".multiple", {std::to_string(targets.size())});
			}
		}

		bool parseTicks(const std::string& text, int& out) {
			char* end = nullptr;
			long  value = std::strtol(text.c_str(), &end, 10);
			if (end == text.c_str() || *end != '\0' || value < 0 || value > 1000000) return false;
			out = static_cast<int>(value);
			return true;
		}

		void runTitle(Server& server, Player& player, const std::vector<std::string>& args) {
			if (args.size() < 2) return usage(server, player, SYNTAX);
			std::vector<Player*> targets = playerTargets(server, player, args[0]);
			if (targets.empty()) return failure(server, player, "argument.entity.notfound.player");
			const std::string& action = args[1];
			auto send = [&](int packetId, Buffer& data) {
				for (Player* target : targets) Packet::send(target->shared_from_this(), packetId, data, server);
			};
			if ((action == "clear" || action == "reset") && args.size() == 2) {
				// ClientboundClearTitlesPacket: reset also puts the display times back to their defaults
				Buffer data;
				data.writeBool(action == "reset");
				send(PacketId::Play::Clientbound::CLEAR_TITLES, data);
				return feedback(server, player, action == "clear" ? "commands.title.cleared" : "commands.title.reset", targets);
			}
			if (action == "times" && args.size() == 5) {
				int fadeIn, stay, fadeOut;
				if (!parseTicks(args[2], fadeIn) || !parseTicks(args[3], stay) || !parseTicks(args[4], fadeOut)) return usage(server, player, SYNTAX);
				Buffer data;
				data.writeInt(fadeIn);
				data.writeInt(stay);
				data.writeInt(fadeOut);
				send(PacketId::Play::Clientbound::SET_TITLES_ANIMATION, data);
				return feedback(server, player, "commands.title.times", targets);
			}
			if ((action == "title" || action == "subtitle" || action == "actionbar") && args.size() >= 3) {
				std::string text;
				for (size_t i = 2; i < args.size(); i++) text += (i > 2 ? " " : "") + args[i];
				Buffer data;
				if (!TextComponent::writeJson(data, text)) return message(server, player, "Invalid JSON text component: " + text);
				int packetId = action == "title"	  ? PacketId::Play::Clientbound::SET_TITLE_TEXT
							   : action == "subtitle" ? PacketId::Play::Clientbound::SET_SUBTITLE_TEXT
													  : PacketId::Play::Clientbound::SET_ACTION_BAR_TEXT;
				send(packetId, data);
				return feedback(server, player, "commands.title.show." + action, targets);
			}
			usage(server, player, SYNTAX);
		}
	} // namespace

	void registerTitle() { commandList().push_back({"title", SYNTAX, runTitle}); }
} // namespace Commands
