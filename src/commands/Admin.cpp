#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <algorithm>

// The admin commands (vanilla's OpCommand, DeOpCommand, KickCommand, BanPlayerCommands, BanIpCommands,
// PardonCommand, PardonIpCommand, BanListCommands, WhitelistCommand), level 3. Players are named: an online one, or
// anyone by its offline UUID (this server has no Mojang authentication)
namespace Commands {
	namespace {
		// The rest of the arguments from index start, joined back with spaces (a reason); fallback if there are none
		std::string joined(const std::vector<std::string>& args, size_t start, const std::string& fallback = "") {
			std::string text;
			for (size_t i = start; i < args.size(); i++) text += (text.empty() ? "" : " ") + args[i];
			return text.empty() ? fallback : text;
		}

		// A named player (GameProfileArgument): its name as written online, its UUID
		struct Profile {
			std::string name;
			UUID		uuid;
			Player*		online = nullptr;
		};

		// The profiles a target names: selectors and online players first, else an offline name
		std::vector<Profile> profiles(Server& server, Player& self, const std::string& target) {
			std::vector<Profile> found;
			for (Player* player : playerTargets(server, self, target)) found.push_back({player->getPlayerName(), player->getUUID(), player});
			if (found.empty() && !target.empty() && target[0] != '@') found.push_back({target, UUID::fromOfflinePlayer(target), nullptr});
			return found;
		}

		// The online players with this UUID
		std::vector<Player*> onlineWith(Server& server, const UUID& uuid) {
			std::vector<Player*> players;
			for (const auto& player : server.getGamePlayers()) {
				if (!player->isDisconnected() && player->getUUID() == uuid) players.push_back(player.get());
			}
			return players;
		}

		// whitelist with enforce-whitelist: the players no longer allowed are kicked (PlayerList.kickUnlistedPlayers)
		void kickUnlisted(Server& server) {
			if (!server.access().whitelistEnabled() || !server.getConfig().getEnforceWhitelist()) return;
			std::vector<Player*> unlisted;
			for (const auto& player : server.getGamePlayers()) {
				if (!player->isDisconnected() && server.access().loginRefusal(player->getUUID(), "")) unlisted.push_back(player.get());
			}
			for (Player* player : unlisted) server.kick(player, "multiplayer.disconnect.not_whitelisted");
		}

		void registerOp() {
			commandList().push_back({"op", "op <targets>", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.size() != 1) return usage(server, player, "op <targets>");
				int changed = 0;
				for (const Profile& profile : profiles(server, player, args[0])) {
					if (!server.access().op(profile.uuid, profile.name)) continue;
					changed++;
					translated(server, player, "commands.op.success", {profile.name});
					for (Player* online : onlineWith(server, profile.uuid)) server.refreshPermissions(*online);
				}
				if (changed == 0) failure(server, player, "commands.op.failed");
			}, LEVEL_ADMINS});
			commandList().push_back({"deop", "deop <targets>", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.size() != 1) return usage(server, player, "deop <targets>");
				int changed = 0;
				for (const Profile& profile : profiles(server, player, args[0])) {
					if (!server.access().deop(profile.uuid)) continue;
					changed++;
					translated(server, player, "commands.deop.success", {profile.name});
					for (Player* online : onlineWith(server, profile.uuid)) server.refreshPermissions(*online);
				}
				if (changed == 0) failure(server, player, "commands.deop.failed");
			}, LEVEL_ADMINS});
		}

		void registerKick() {
			commandList().push_back({"kick", "kick <targets> [<reason>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.empty()) return usage(server, player, "kick <targets> [<reason>]");
				std::vector<Player*> targets = playerTargets(server, player, args[0]);
				if (targets.empty()) return failure(server, player, "argument.entity.notfound.player");
				std::string reason = joined(args, 1);
				for (Player* target : targets) {
					std::string name = target->getPlayerName();
					if (reason.empty()) {
						server.kick(target, "multiplayer.disconnect.kicked");
						translated(server, player, "commands.kick.success", {name, "Kicked by an operator."});
					} else {
						server.kickWithText(target, reason);
						translated(server, player, "commands.kick.success", {name, reason});
					}
				}
			}, LEVEL_ADMINS});
		}

		void registerBans() {
			commandList().push_back({"ban", "ban <targets> [<reason>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.empty()) return usage(server, player, "ban <targets> [<reason>]");
				std::string reason	= joined(args, 1, "Banned by an operator.");
				int			changed = 0;
				for (const Profile& profile : profiles(server, player, args[0])) {
					if (!server.access().ban(profile.uuid, profile.name, player.getPlayerName(), reason)) continue;
					changed++;
					translated(server, player, "commands.ban.success", {profile.name, reason});
					for (Player* online : onlineWith(server, profile.uuid)) server.kick(online, "multiplayer.disconnect.banned");
				}
				if (changed == 0) failure(server, player, "commands.ban.failed");
			}, LEVEL_ADMINS});
			commandList().push_back({"ban-ip", "ban-ip <target> [<reason>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.empty()) return usage(server, player, "ban-ip <target> [<reason>]");
				// An IP, or an online player's
				std::string ip = args[0];
				if (!ServerAccess::isIpAddress(ip)) {
					auto targets = server.findPlayersByName(ip);
					if (targets.empty() || targets[0]->address().empty()) return failure(server, player, "commands.banip.invalid");
					ip = targets[0]->address();
				}
				std::string reason = joined(args, 1, "Banned by an operator.");
				if (!server.access().banIp(ip, player.getPlayerName(), reason)) return failure(server, player, "commands.banip.failed");
				translated(server, player, "commands.banip.success", {ip, reason});
				std::vector<Player*> affected;
				std::string			 names;
				for (const auto& online : server.getGamePlayers()) {
					if (online->isDisconnected() || online->address() != ip) continue;
					affected.push_back(online.get());
					names += (names.empty() ? "" : ", ") + online->getPlayerName();
				}
				if (!affected.empty()) translated(server, player, "commands.banip.info", {std::to_string(affected.size()), names});
				for (Player* online : affected) server.kick(online, "multiplayer.disconnect.ip_banned");
			}, LEVEL_ADMINS});
			commandList().push_back({"pardon", "pardon <targets>", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.size() != 1) return usage(server, player, "pardon <targets>");
				if (!server.access().pardon(args[0])) return failure(server, player, "commands.pardon.failed");
				translated(server, player, "commands.pardon.success", {args[0]});
			}, LEVEL_ADMINS});
			commandList().push_back({"pardon-ip", "pardon-ip <target>", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.size() != 1) return usage(server, player, "pardon-ip <target>");
				if (!ServerAccess::isIpAddress(args[0])) return failure(server, player, "commands.pardonip.invalid");
				if (!server.access().pardonIp(args[0])) return failure(server, player, "commands.pardonip.failed");
				translated(server, player, "commands.pardonip.success", {args[0]});
			}, LEVEL_ADMINS});
			commandList().push_back({"banlist", "banlist [ips|players]", [](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.size() > 1 || (args.size() == 1 && args[0] != "ips" && args[0] != "players")) return usage(server, player, "banlist [ips|players]");
				std::vector<ServerAccess::Ban> bans;
				if (args.empty() || args[0] == "players") bans = server.access().bans();
				if (args.empty() || args[0] == "ips") {
					auto ips = server.access().ipBans();
					bans.insert(bans.end(), ips.begin(), ips.end());
				}
				if (bans.empty()) return translated(server, player, "commands.banlist.none");
				translated(server, player, "commands.banlist.list", {std::to_string(bans.size())});
				for (const auto& ban : bans) translated(server, player, "commands.banlist.entry", {ban.name.empty() ? ban.target : ban.name, ban.source, ban.reason});
			}, LEVEL_ADMINS});
		}

		void registerWhitelist() {
			commandList().push_back({"whitelist", "whitelist (on|off|list|reload|add <targets>|remove <targets>)",
									 [](Server& server, Player& player, const std::vector<std::string>& args) {
				const std::string syntax = "whitelist (on|off|list|reload|add <targets>|remove <targets>)";
				if (args.empty()) return usage(server, player, syntax);
				ServerAccess& access = server.access();
				const std::string& sub = args[0];
				if ((sub == "on" || sub == "off" || sub == "list" || sub == "reload") && args.size() != 1) return usage(server, player, syntax);
				if (sub == "on") {
					if (access.whitelistEnabled()) return failure(server, player, "commands.whitelist.alreadyOn");
					access.setWhitelistEnabled(true);
					server.getConfig().setWhiteList(true);
					translated(server, player, "commands.whitelist.enabled");
					kickUnlisted(server);
				} else if (sub == "off") {
					if (!access.whitelistEnabled()) return failure(server, player, "commands.whitelist.alreadyOff");
					access.setWhitelistEnabled(false);
					server.getConfig().setWhiteList(false);
					translated(server, player, "commands.whitelist.disabled");
				} else if (sub == "list") {
					auto list = access.whitelist();
					if (list.empty()) return translated(server, player, "commands.whitelist.none");
					std::string names;
					for (const auto& entry : list) names += (names.empty() ? "" : ", ") + entry.name;
					translated(server, player, "commands.whitelist.list", {std::to_string(list.size()), names});
				} else if (sub == "reload") {
					access.reloadWhitelist();
					translated(server, player, "commands.whitelist.reloaded");
					kickUnlisted(server);
				} else if ((sub == "add" || sub == "remove") && args.size() == 2) {
					bool add	 = sub == "add";
					int	 changed = 0;
					for (const Profile& profile : profiles(server, player, args[1])) {
						if (add ? !access.addToWhitelist(profile.uuid, profile.name) : !access.removeFromWhitelist(profile.uuid)) continue;
						changed++;
						translated(server, player, add ? "commands.whitelist.add.success" : "commands.whitelist.remove.success", {profile.name});
					}
					if (changed == 0) return failure(server, player, add ? "commands.whitelist.add.failed" : "commands.whitelist.remove.failed");
					if (!add) kickUnlisted(server);
				} else {
					usage(server, player, syntax);
				}
			}, LEVEL_ADMINS});
		}
	} // namespace

	void registerAdmin() {
		registerOp();
		registerKick();
		registerBans();
		registerWhitelist();
	}

	// StopCommand and SaveAllCommand (level 4)
	void registerServer() {
		commandList().push_back({"stop", "stop", [](Server& server, Player& player, const std::vector<std::string>& args) {
			if (!args.empty()) return usage(server, player, "stop");
			translated(server, player, "commands.stop.stopping");
			server.requestStop();
		}, LEVEL_OWNERS});
		commandList().push_back({"save-all", "save-all", [](Server& server, Player& player, const std::vector<std::string>& args) {
			if (!args.empty() && !(args.size() == 1 && args[0] == "flush")) return usage(server, player, "save-all [flush]");
			translated(server, player, "commands.save.saving");
			server.saveAll();
			translated(server, player, "commands.save.success");
		}, LEVEL_OWNERS});
	}

	void registerChat() {
		// SayCommand (level 2): "[name] message" for everyone
		commandList().push_back({"say", "say <message>", [](Server& server, Player& player, const std::vector<std::string>& args) {
			if (args.empty()) return usage(server, player, "say <message>");
			broadcastTranslated(server, "chat.type.announcement", {player.getPlayerName(), joined(args, 0)});
		}});
		// MsgCommand (level 0): a private message, as /tell, /msg and /w
		for (const char* name : {"tell", "msg", "w"}) {
			std::string syntax = std::string(name) + " <targets> <message>";
			commandList().push_back({name, syntax, [syntax](Server& server, Player& player, const std::vector<std::string>& args) {
				if (args.size() < 2) return usage(server, player, syntax);
				std::vector<Player*> targets = playerTargets(server, player, args[0]);
				if (targets.empty()) return failure(server, player, "argument.entity.notfound.player");
				std::string text = joined(args, 1);
				for (Player* target : targets) {
					translated(server, player, "commands.message.display.outgoing", {target->getPlayerName(), text});
					if (target != &player) translated(server, *target, "commands.message.display.incoming", {player.getPlayerName(), text});
				}
			}, LEVEL_ALL});
		}
		// EmoteCommands (level 0): "* name action" for everyone
		commandList().push_back({"me", "me <action>", [](Server& server, Player& player, const std::vector<std::string>& args) {
			if (args.empty()) return usage(server, player, "me <action>");
			broadcastTranslated(server, "chat.type.emote", {player.getPlayerName(), joined(args, 0)});
		}, LEVEL_ALL});
	}
} // namespace Commands
