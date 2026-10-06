#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/entity/MobEffects.hpp"

#include <cstdlib>

// EffectCommands: /effect give <targets> <effect> [<seconds>|infinite] [<amplifier>] [<hideParticles>], and
// /effect clear [<targets>] [<effect>]. The targets are players (a name, @s, @a, @p)
namespace {
	std::vector<Player*> targets(Server& server, Player& self, const std::string& selector) {
		std::vector<Player*> found;
		if (selector == "@s" || selector == "@p") {
			found.push_back(&self);
		} else if (selector == "@a") {
			for (const auto& player : server.getGamePlayers()) {
				if (!player->isDisconnected()) found.push_back(player.get());
			}
		} else {
			for (const auto& player : server.findPlayersByName(selector)) found.push_back(player.get());
		}
		return found;
	}

	bool parseInt(const std::string& text, long min, long max, int& out) {
		char* end	= nullptr;
		long  value = std::strtol(text.c_str(), &end, 10);
		if (end == text.c_str() || *end != '\0' || value < min || value > max) return false;
		out = static_cast<int>(value);
		return true;
	}

	std::string effectName(MobEffects::Effect effect) {
		std::string name = MobEffects::info(effect).name;
		return name.substr(name.find(':') + 1);
	}
} // namespace

namespace Commands {
	void registerEffect() {
		const char* usageText = "effect give <targets> <effect> [<seconds>|infinite] [<amplifier>] [<hideParticles>] | effect clear [<targets>] [<effect>]";
		commandList().push_back({"effect", usageText, [usageText](Server& server, Player& player, const std::vector<std::string>& args) {
									 if (args.empty() || (args[0] != "give" && args[0] != "clear")) {
										 usage(server, player, usageText);
										 return;
									 }
									 if (args[0] == "clear") {
										 std::vector<Player*> who = args.size() >= 2 ? targets(server, player, args[1]) : std::vector<Player*>{&player};
										 MobEffects::Effect	  effect = MobEffects::Count;
										 if (args.size() >= 3) {
											 effect = MobEffects::byName(args[2]);
											 if (effect == MobEffects::Count) {
												 message(server, player, "Unknown effect: " + args[2]);
												 return;
											 }
										 }
										 int cleared = 0;
										 for (Player* target : who) {
											 cleared += effect == MobEffects::Count ? target->removeAllEffects() : target->removeEffect(effect);
										 }
										 if (cleared == 0) {
											 message(server, player, "No effect was removed");
										 } else {
											 message(server, player, "Removed " + (effect == MobEffects::Count ? std::string("every effect") : effectName(effect)) + " from " +
																			 std::to_string(cleared) + " target(s)");
										 }
										 return;
									 }

									 if (args.size() < 3 || args.size() > 6) {
										 usage(server, player, usageText);
										 return;
									 }
									 std::vector<Player*> who	 = targets(server, player, args[1]);
									 MobEffects::Effect	  effect = MobEffects::byName(args[2]);
									 if (who.empty()) {
										 message(server, player, "Player not found: " + args[1]);
										 return;
									 }
									 if (effect == MobEffects::Count) {
										 message(server, player, "Unknown effect: " + args[2]);
										 return;
									 }
									 bool instant  = MobEffects::info(effect).instantaneous;
									 int  duration = instant ? 1 : 600; // Seconds: 30 by default, 1 tick for the instant ones
									 int  amplifier = 0;
									 bool hide		= false;
									 if (args.size() >= 4) {
										 int seconds = 0;
										 if (args[3] == "infinite") {
											 duration = MobEffects::INFINITE_DURATION;
										 } else if (parseInt(args[3], 1, 1000000, seconds)) {
											 duration = instant ? seconds : seconds * 20;
										 } else {
											 usage(server, player, usageText);
											 return;
										 }
									 }
									 if (args.size() >= 5 && !parseInt(args[4], 0, 255, amplifier)) {
										 usage(server, player, usageText);
										 return;
									 }
									 if (args.size() == 6) {
										 if (args[5] != "true" && args[5] != "false") {
											 usage(server, player, usageText);
											 return;
										 }
										 hide = args[5] == "true";
									 }
									 int applied = 0;
									 for (Player* target : who) {
										 applied += target->addEffect(MobEffects::Instance(effect, duration, amplifier, false, !hide), &player);
									 }
									 if (applied == 0) {
										 message(server, player, "Unable to apply this effect (target is either immune to effects, or has something stronger)");
									 } else {
										 message(server, player, "Applied effect " + effectName(effect) + " to " + std::to_string(applied) + " target(s)");
									 }
								 }});
	}
} // namespace Commands
