#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Xp.hpp"

#include <cmath>
#include <cstdlib>

// ExperienceCommand: /experience (and /xp) add|set <targets> <amount> [points|levels], query <target> points|levels
namespace {
	const char* USAGE = "xp add|set <targets> <amount> [points|levels] | xp query <target> points|levels";

	bool parseInt(const std::string& text, int& out) {
		char* end	= nullptr;
		long  value = std::strtol(text.c_str(), &end, 10);
		if (end == text.c_str() || *end != '\0' || value < -1000000000L || value > 1000000000L) return false;
		out = static_cast<int>(value);
		return true;
	}

	void runExperience(Server& server, Player& player, const std::vector<std::string>& args) {
		using namespace Commands;
		if (args.size() < 2) {
			usage(server, player, USAGE);
			return;
		}
		std::vector<Player*> targets = playerTargets(server, player, args[1]);
		if (targets.empty()) {
			message(server, player, "No player was found");
			return;
		}
		const std::string& action = args[0];
		if (action == "query") {
			if (args.size() != 3 || (args[2] != "points" && args[2] != "levels")) {
				usage(server, player, USAGE);
				return;
			}
			Player* target = targets[0];
			int		value  = args[2] == "levels" ? target->getXpLevel()
												 : static_cast<int>(std::lround(target->getXpProgress() * Xp::xpNeededForNextLevel(target->getXpLevel())));
			message(server, player, target->getPlayerName() + " has " + std::to_string(value) + " experience " + args[2]);
			return;
		}
		int	 amount = 0;
		bool levels = args.size() == 4 && args[3] == "levels";
		if ((action != "add" && action != "set") || args.size() < 3 || args.size() > 4 || !parseInt(args[2], amount) ||
			(args.size() == 4 && args[3] != "points" && args[3] != "levels")) {
			usage(server, player, USAGE);
			return;
		}
		for (Player* target : targets) {
			if (action == "add") {
				if (levels) {
					Xp::addLevels(server, *target, amount);
				} else {
					Xp::addExperience(server, *target, amount);
				}
			} else if (levels) {
				// set levels: the progress within the level is kept
				Xp::addLevels(server, *target, std::max(0, amount) - target->getXpLevel());
			} else {
				// set points: the points into the current level (fewer than it takes to the next one)
				int needed = Xp::xpNeededForNextLevel(target->getXpLevel());
				if (amount < 0 || amount >= needed) {
					message(server, player, "Can't set experience points above the maximum points for the player's current level (" + std::to_string(needed - 1) + ")");
					return;
				}
				int current = target->getXpTotal() - Xp::xpToLevel(target->getXpLevel());
				Xp::addExperience(server, *target, amount - current);
			}
		}
		std::string what = levels ? "levels" : "points";
		std::string who	 = targets.size() == 1 ? targets[0]->getPlayerName() : std::to_string(targets.size()) + " players";
		message(server, player, (action == "add" ? "Gave " : "Set ") + std::to_string(amount) + " experience " + what + (action == "add" ? " to " : " on ") + who);
	}
} // namespace

namespace Commands {
	void registerExperience() {
		commandList().push_back({"experience", USAGE, runExperience});
		commandList().push_back({"xp", USAGE, runExperience});
	}
} // namespace Commands
