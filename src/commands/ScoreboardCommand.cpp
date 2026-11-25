#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Scoreboard.hpp"

#include <cstdlib>

// ScoreboardCommand: objectives (list, add, remove, setdisplay, modify) and players (list, get, set, add, remove,
// reset). Score holders are players (selectors) or any name; * is every tracked holder
namespace Commands {
	namespace {
		const std::string SYNTAX = "scoreboard (objectives (list|add <objective> <criteria> [<displayName>]|remove <objective>|setdisplay <slot> [<objective>]|"
								   "modify <objective> (displayname <text>|rendertype (hearts|integer)))|players (list [<target>]|get <target> <objective>|"
								   "set|add|remove <targets> <objective> <score>|reset <targets> [<objective>]))";

		bool integer(const std::string& text, int& out) {
			char* end	= nullptr;
			long  value = std::strtol(text.c_str(), &end, 10);
			if (end == text.c_str() || *end != '\0' || value < -2147483648L || value > 2147483647L) return false;
			out = static_cast<int>(value);
			return true;
		}

		std::string joined(const std::vector<std::string>& args, size_t start) {
			std::string text;
			for (size_t i = start; i < args.size(); i++) text += (text.empty() ? "" : " ") + args[i];
			return text;
		}

		// The score holders a target names: players for selectors, every tracked holder for *, else the name itself
		std::vector<std::string> holders(Server& server, Player& source, const std::string& target) {
			std::vector<std::string> names;
			if (target == "*") return server.scoreboard().owners();
			if (!target.empty() && target[0] == '@') {
				for (Player* player : playerTargets(server, source, target)) names.push_back(player->getPlayerName());
				return names;
			}
			return {target};
		}

		const Scoreboard::Objective* objectiveNamed(Server& server, Player& player, const std::string& name) {
			const Scoreboard::Objective* objective = server.scoreboard().objective(name);
			if (!objective) failure(server, player, "arguments.objective.notFound", {name});
			return objective;
		}

		void objectives(Server& server, Player& player, const std::vector<std::string>& args) {
			Scoreboard& board = server.scoreboard();
			const std::string& sub = args.size() > 1 ? args[1] : "";
			if (sub == "list" && args.size() == 2) {
				auto list = board.objectives();
				if (list.empty()) return translated(server, player, "commands.scoreboard.objectives.list.empty");
				std::string names;
				for (const auto* objective : list) names += (names.empty() ? "[" : ", [") + objective->displayName + "]";
				return translated(server, player, "commands.scoreboard.objectives.list.success", {std::to_string(list.size()), names});
			}
			if (sub == "add" && args.size() >= 4) {
				if (!Scoreboard::isCriterion(args[3])) return failure(server, player, "argument.criteria.invalid", {args[3]});
				if (args[2].size() > 16) return usage(server, player, SYNTAX);
				if (!board.addObjective(args[2], args[3], joined(args, 4))) return failure(server, player, "commands.scoreboard.objectives.add.duplicate");
				return translated(server, player, "commands.scoreboard.objectives.add.success", {"[" + board.objective(args[2])->displayName + "]"});
			}
			if (sub == "remove" && args.size() == 3) {
				const Scoreboard::Objective* objective = objectiveNamed(server, player, args[2]);
				if (!objective) return;
				std::string name = "[" + objective->displayName + "]";
				board.removeObjective(args[2]);
				return translated(server, player, "commands.scoreboard.objectives.remove.success", {name});
			}
			if (sub == "setdisplay" && (args.size() == 3 || args.size() == 4)) {
				int slot = Scoreboard::slotByName(args[2]);
				if (slot < 0) return failure(server, player, "argument.scoreboardDisplaySlot.invalid", {args[2]});
				if (args.size() == 3) {
					if (board.display(slot).empty()) return failure(server, player, "commands.scoreboard.objectives.display.alreadyEmpty");
					board.setDisplay(slot, "");
					return translated(server, player, "commands.scoreboard.objectives.display.cleared", {args[2]});
				}
				const Scoreboard::Objective* objective = objectiveNamed(server, player, args[3]);
				if (!objective) return;
				if (board.display(slot) == args[3]) return failure(server, player, "commands.scoreboard.objectives.display.alreadySet");
				board.setDisplay(slot, args[3]);
				return translated(server, player, "commands.scoreboard.objectives.display.set", {args[2], "[" + objective->displayName + "]"});
			}
			if (sub == "modify" && args.size() >= 5) {
				const Scoreboard::Objective* objective = objectiveNamed(server, player, args[2]);
				if (!objective) return;
				if (args[3] == "displayname") {
					board.setDisplayName(args[2], joined(args, 4));
					return translated(server, player, "commands.scoreboard.objectives.modify.displayname", {args[2], joined(args, 4)});
				}
				if (args[3] == "rendertype" && args.size() == 5 && (args[4] == "hearts" || args[4] == "integer")) {
					board.setHearts(args[2], args[4] == "hearts");
					return translated(server, player, "commands.scoreboard.objectives.modify.rendertype", {"[" + objective->displayName + "]"});
				}
			}
			usage(server, player, SYNTAX);
		}

		void players(Server& server, Player& player, const std::vector<std::string>& args) {
			Scoreboard& board = server.scoreboard();
			const std::string& sub = args.size() > 1 ? args[1] : "";
			if (sub == "list" && args.size() == 2) {
				auto owners = board.owners();
				if (owners.empty()) return translated(server, player, "commands.scoreboard.players.list.empty");
				std::string names;
				for (const std::string& owner : owners) names += (names.empty() ? "" : ", ") + owner;
				return translated(server, player, "commands.scoreboard.players.list.success", {std::to_string(owners.size()), names});
			}
			if (sub == "list" && args.size() == 3) {
				for (const std::string& owner : holders(server, player, args[2])) {
					auto scores = board.scoresOf(owner);
					if (scores.empty()) {
						translated(server, player, "commands.scoreboard.players.list.entity.empty", {owner});
						continue;
					}
					translated(server, player, "commands.scoreboard.players.list.entity.success", {owner, std::to_string(scores.size())});
					for (const auto& [objective, value] : scores) {
						const Scoreboard::Objective* o = board.objective(objective);
						translated(server, player, "commands.scoreboard.players.list.entity.entry", {"[" + (o ? o->displayName : objective) + "]", std::to_string(value)});
					}
				}
				return;
			}
			if (sub == "get" && args.size() == 4) {
				const Scoreboard::Objective* objective = objectiveNamed(server, player, args[3]);
				if (!objective) return;
				auto names = holders(server, player, args[2]);
				if (names.size() != 1) return failure(server, player, "argument.player.toomany");
				std::optional<int> value = board.score(names[0], args[3]);
				if (!value) return failure(server, player, "commands.scoreboard.players.get.null", {args[3], names[0]});
				return translated(server, player, "commands.scoreboard.players.get.success", {names[0], std::to_string(*value), "[" + objective->displayName + "]"});
			}
			if ((sub == "set" || sub == "add" || sub == "remove") && args.size() == 5) {
				const Scoreboard::Objective* objective = objectiveNamed(server, player, args[3]);
				if (!objective) return;
				if (Scoreboard::isReadOnly(objective->criterion)) return failure(server, player, "commands.scoreboard.objectives.readonly", {args[3]});
				int amount;
				if (!integer(args[4], amount) || (sub != "set" && amount < 0)) return usage(server, player, SYNTAX);
				auto names = holders(server, player, args[2]);
				if (names.empty()) return failure(server, player, "argument.entity.notfound.entity");
				int last = 0;
				for (const std::string& owner : names) {
					if (sub == "set") {
						board.setScore(owner, args[3], amount);
						last = amount;
					} else {
						last = board.addScore(owner, args[3], sub == "add" ? amount : -amount);
					}
				}
				std::string name = "[" + objective->displayName + "]";
				bool		single = names.size() == 1;
				if (sub == "set") {
					return translated(server, player, single ? "commands.scoreboard.players.set.success.single" : "commands.scoreboard.players.set.success.multiple",
									  {name, single ? names[0] : std::to_string(names.size()), std::to_string(amount)});
				}
				std::string key = "commands.scoreboard.players." + sub + ".success." + (single ? "single" : "multiple");
				if (single) return translated(server, player, key, {std::to_string(amount), name, names[0], std::to_string(last)});
				return translated(server, player, key, {std::to_string(amount), name, std::to_string(names.size())});
			}
			if (sub == "reset" && (args.size() == 3 || args.size() == 4)) {
				auto names = holders(server, player, args[2]);
				if (names.empty()) return failure(server, player, "argument.entity.notfound.entity");
				if (args.size() == 4 && !objectiveNamed(server, player, args[3])) return;
				for (const std::string& owner : names) {
					if (args.size() == 4) {
						board.resetScore(owner, args[3]);
					} else {
						board.resetScores(owner);
					}
				}
				if (args.size() == 4) return translated(server, player, "commands.scoreboard.players.reset.specific.single", {args[3], names.size() == 1 ? names[0] : std::to_string(names.size())});
				return translated(server, player, "commands.scoreboard.players.reset.all.single", {names.size() == 1 ? names[0] : std::to_string(names.size())});
			}
			usage(server, player, SYNTAX);
		}

		void runScoreboard(Server& server, Player& player, const std::vector<std::string>& args) {
			if (!args.empty() && args[0] == "objectives") return objectives(server, player, args);
			if (!args.empty() && args[0] == "players") return players(server, player, args);
			usage(server, player, SYNTAX);
		}
	} // namespace

	void registerScoreboard() { commandList().push_back({"scoreboard", SYNTAX, runScoreboard}); }
} // namespace Commands
