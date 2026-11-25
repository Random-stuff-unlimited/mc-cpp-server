#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Scoreboard.hpp"

// TeamCommand: add, remove, empty, join, leave, modify, list
namespace Commands {
	namespace {
		const std::string SYNTAX = "team (list [<team>]|add <team> [<displayName>]|remove <team>|empty <team>|join <team> [<members>]|leave <members>|"
								   "modify <team> <option> <value>)";

		std::string joined(const std::vector<std::string>& args, size_t start) {
			std::string text;
			for (size_t i = start; i < args.size(); i++) text += (text.empty() ? "" : " ") + args[i];
			return text;
		}

		// Members: players for selectors, else the name itself
		std::vector<std::string> members(Server& server, Player& source, const std::string& target) {
			std::vector<std::string> names;
			if (!target.empty() && target[0] == '@') {
				for (Player* player : playerTargets(server, source, target)) names.push_back(player->getPlayerName());
				return names;
			}
			return {target};
		}

		void runTeam(Server& server, Player& player, const std::vector<std::string>& args) {
			Scoreboard&		   board = server.scoreboard();
			const std::string& sub	 = args.empty() ? "" : args[0];
			auto			   named = [&](const std::string& name) {
				  const Scoreboard::Team* team = board.team(name);
				  if (!team) failure(server, player, "team.notFound", {name});
				  return team;
			};
			if (sub == "list" && args.size() == 1) {
				auto teams = board.teams();
				if (teams.empty()) return translated(server, player, "commands.team.list.teams.empty");
				std::string names;
				for (const auto* team : teams) names += (names.empty() ? "[" : ", [") + team->displayName + "]";
				return translated(server, player, "commands.team.list.teams.success", {std::to_string(teams.size()), names});
			}
			if (sub == "list" && args.size() == 2) {
				const Scoreboard::Team* team = named(args[1]);
				if (!team) return;
				if (team->members.empty()) return translated(server, player, "commands.team.list.members.empty", {"[" + team->displayName + "]"});
				std::string names;
				for (const std::string& member : team->members) names += (names.empty() ? "" : ", ") + member;
				return translated(server, player, "commands.team.list.members.success", {"[" + team->displayName + "]", std::to_string(team->members.size()), names});
			}
			if (sub == "add" && args.size() >= 2) {
				if (!board.addTeam(args[1], joined(args, 2))) return failure(server, player, "commands.team.add.duplicate");
				return translated(server, player, "commands.team.add.success", {"[" + board.team(args[1])->displayName + "]"});
			}
			if ((sub == "remove" || sub == "empty") && args.size() == 2) {
				const Scoreboard::Team* team = named(args[1]);
				if (!team) return;
				std::string name = "[" + team->displayName + "]";
				if (sub == "remove") {
					board.removeTeam(args[1]);
					return translated(server, player, "commands.team.remove.success", {name});
				}
				std::vector<std::string> list = team->members;
				if (list.empty()) return failure(server, player, "commands.team.empty.unchanged");
				for (const std::string& member : list) board.leave(member);
				return translated(server, player, "commands.team.empty.success", {std::to_string(list.size()), name});
			}
			if (sub == "join" && (args.size() == 2 || args.size() == 3)) {
				const Scoreboard::Team* team = named(args[1]);
				if (!team) return;
				std::vector<std::string> list = args.size() == 3 ? members(server, player, args[2]) : std::vector<std::string>{player.getPlayerName()};
				if (list.empty()) return failure(server, player, "argument.entity.notfound.entity");
				for (const std::string& member : list) board.join(args[1], member);
				std::string name = "[" + board.team(args[1])->displayName + "]";
				if (list.size() == 1) return translated(server, player, "commands.team.join.success.single", {list[0], name});
				return translated(server, player, "commands.team.join.success.multiple", {std::to_string(list.size()), name});
			}
			if (sub == "leave" && args.size() == 2) {
				std::vector<std::string> list = members(server, player, args[1]);
				int						 left = 0;
				for (const std::string& member : list) left += board.leave(member);
				if (left == 0) return failure(server, player, "argument.entity.notfound.entity");
				if (list.size() == 1) return translated(server, player, "commands.team.leave.success.single", {list[0]});
				return translated(server, player, "commands.team.leave.success.multiple", {std::to_string(left)});
			}
			if (sub == "modify" && args.size() >= 3) {
				const Scoreboard::Team* team = named(args[1]);
				if (!team) return;
				std::string value = joined(args, 3);
				if (!board.modifyTeam(args[1], args[2], value)) return usage(server, player, SYNTAX);
				std::string name = "[" + board.team(args[1])->displayName + "]";
				static const std::vector<std::pair<std::string, std::string>> KEYS = {
					{"color", "commands.team.option.color.success"},
					{"prefix", "commands.team.option.prefix.success"},
					{"suffix", "commands.team.option.suffix.success"},
					{"displayName", "commands.team.option.name.success"},
					{"friendlyFire", "commands.team.option.friendlyfire.enabled"},
					{"seeFriendlyInvisibles", "commands.team.option.seeFriendlyInvisibles.enabled"},
					{"nametagVisibility", "commands.team.option.nametagVisibility.success"},
					{"collisionRule", "commands.team.option.collisionRule.success"},
				};
				for (const auto& [option, key] : KEYS) {
					if (option != args[2]) continue;
					std::string message = key;
					if (value == "false") message.replace(message.rfind("enabled"), 7, "disabled");
					if (option == "prefix" || option == "suffix") return translated(server, player, message, {value});
					if (option == "nametagVisibility" || option == "collisionRule" || option == "color") return translated(server, player, message, {name, value});
					return translated(server, player, message, {name});
				}
				return;
			}
			usage(server, player, SYNTAX);
		}
	} // namespace

	void registerTeam() { commandList().push_back({"team", SYNTAX, runTeam}); }
} // namespace Commands
