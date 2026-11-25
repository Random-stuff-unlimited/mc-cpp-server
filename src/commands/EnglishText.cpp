#include "Commands.hpp"

#include <cctype>
#include <unordered_map>

// The en_us wording of the translation keys the commands send, for the console (the client translates them for the
// players). Copied from the game's en_us.json
namespace Commands {
	namespace {
		const std::unordered_map<std::string, std::string>& english() {
			static const std::unordered_map<std::string, std::string> TEXT = {
				{"argument.entity.notfound.player", "No player was found"},
				{"chat.type.announcement", "[%s] %s"},
				{"chat.type.emote", "* %s %s"},
				{"chat.type.text", "<%s> %s"},
				{"commands.ban.failed", "Nothing changed. The player is already banned"},
				{"commands.ban.success", "Banned %s: %s"},
				{"server.tempban.success", "Banned %s until %s: %s"},
				{"server.tempban.invalid", "Invalid duration (a number then s, m, h, d or w, e.g. 7d)"},
				{"commands.banip.failed", "Nothing changed. That IP is already banned"},
				{"commands.banip.info", "This ban affects %s player(s): %s"},
				{"commands.banip.invalid", "Invalid IP address or unknown player"},
				{"commands.banip.success", "Banned IP %s: %s"},
				{"commands.banlist.entry", "%s was banned by %s: %s"},
				{"commands.banlist.list", "There are %s ban(s):"},
				{"commands.banlist.none", "There are no bans"},
				{"commands.deop.failed", "Nothing changed. The player is not an operator"},
				{"commands.deop.success", "Made %s no longer a server operator"},
				{"commands.kick.success", "Kicked %s: %s"},
				{"commands.message.display.incoming", "%s whispers to you: %s"},
				{"commands.message.display.outgoing", "You whisper to %s: %s"},
				{"commands.op.failed", "Nothing changed. The player already is an operator"},
				{"commands.op.success", "Made %s a server operator"},
				{"commands.pardon.failed", "Nothing changed. The player isn't banned"},
				{"commands.pardon.success", "Unbanned %s"},
				{"commands.pardonip.failed", "Nothing changed. That IP isn't banned"},
				{"commands.pardonip.invalid", "Invalid IP address"},
				{"commands.pardonip.success", "Unbanned IP %s"},
				{"commands.save.saving", "Saving the game (this may take a moment!)"},
				{"commands.save.success", "Saved the game"},
				{"commands.stop.stopping", "Stopping the server"},
				{"arguments.objective.notFound", "Unknown scoreboard objective '%s'"},
				{"argument.criteria.invalid", "Unknown criterion '%s'"},
				{"argument.scoreboardDisplaySlot.invalid", "Unknown display slot '%s'"},
				{"commands.scoreboard.objectives.add.duplicate", "An objective already exists by that name"},
				{"commands.scoreboard.objectives.add.success", "Created new objective %s"},
				{"commands.scoreboard.objectives.display.alreadyEmpty", "Nothing changed. That display slot is already empty"},
				{"commands.scoreboard.objectives.display.alreadySet", "Nothing changed. That display slot is already showing that objective"},
				{"commands.scoreboard.objectives.display.cleared", "Cleared any objectives in display slot %s"},
				{"commands.scoreboard.objectives.display.set", "Set display slot %s to show objective %s"},
				{"commands.scoreboard.objectives.list.empty", "There are no objectives"},
				{"commands.scoreboard.objectives.list.success", "There are %s objective(s): %s"},
				{"commands.scoreboard.objectives.modify.displayname", "Changed the display name of %s to %s"},
				{"commands.scoreboard.objectives.modify.rendertype", "Changed the render type of objective %s"},
				{"commands.scoreboard.objectives.readonly", "The objective %s is read-only"},
				{"commands.scoreboard.objectives.remove.success", "Removed objective %s"},
				{"commands.scoreboard.players.add.success.multiple", "Added %s to %s for %s entities"},
				{"commands.scoreboard.players.add.success.single", "Added %s to %s for %s (now %s)"},
				{"commands.scoreboard.players.get.null", "Can't get value of %s for %s; none is set"},
				{"commands.scoreboard.players.get.success", "%s has %s %s"},
				{"commands.scoreboard.players.list.empty", "There are no tracked entities"},
				{"commands.scoreboard.players.list.entity.empty", "%s has no scores to show"},
				{"commands.scoreboard.players.list.entity.entry", "%s: %s"},
				{"commands.scoreboard.players.list.entity.success", "%s has %s score(s):"},
				{"commands.scoreboard.players.list.success", "There are %s tracked entity/entities: %s"},
				{"commands.scoreboard.players.remove.success.multiple", "Removed %s from %s for %s entities"},
				{"commands.scoreboard.players.remove.success.single", "Removed %s from %s for %s (now %s)"},
				{"commands.scoreboard.players.reset.all.single", "Reset all scores for %s"},
				{"commands.scoreboard.players.reset.specific.single", "Reset %s for %s"},
				{"commands.scoreboard.players.set.success.multiple", "Set %s for %s entities to %s"},
				{"commands.scoreboard.players.set.success.single", "Set %s for %s to %s"},
				{"commands.team.add.duplicate", "A team already exists by that name"},
				{"commands.team.add.success", "Created team %s"},
				{"commands.team.empty.success", "Removed %s member(s) from team %s"},
				{"commands.team.empty.unchanged", "Nothing changed. That team is already empty"},
				{"commands.team.join.success.multiple", "Added %s members to team %s"},
				{"commands.team.join.success.single", "Added %s to team %s"},
				{"commands.team.leave.success.multiple", "Removed %s members from any team"},
				{"commands.team.leave.success.single", "Removed %s from any team"},
				{"commands.team.list.members.empty", "There are no members on team %s"},
				{"commands.team.list.members.success", "Team %s has %s member(s): %s"},
				{"commands.team.list.teams.empty", "There are no teams"},
				{"commands.team.list.teams.success", "There are %s team(s): %s"},
				{"commands.team.option.collisionRule.success", "Collision rule for team %s is now \"%s\""},
				{"commands.team.option.color.success", "Updated the color for team %s to %s"},
				{"commands.team.option.friendlyfire.disabled", "Disabled friendly fire for team %s"},
				{"commands.team.option.friendlyfire.enabled", "Enabled friendly fire for team %s"},
				{"commands.team.option.name.success", "Updated the name of team %s"},
				{"commands.team.option.nametagVisibility.success", "Nametag visibility for team %s is now \"%s\""},
				{"commands.team.option.prefix.success", "Team prefix set to %s"},
				{"commands.team.option.seeFriendlyInvisibles.disabled", "Team %s can no longer see invisible teammates"},
				{"commands.team.option.seeFriendlyInvisibles.enabled", "Team %s can now see invisible teammates"},
				{"commands.team.option.suffix.success", "Team suffix set to %s"},
				{"commands.team.remove.success", "Removed team %s"},
				{"team.notFound", "Unknown team '%s'"},
				{"commands.playsound.success.multiple", "Played sound %s to %s players"},
				{"commands.playsound.success.single", "Played sound %s to %s"},
				{"commands.rotate.success", "Rotated %s"},
				{"commands.stopsound.success.source.any", "Stopped all '%s' sounds"},
				{"commands.stopsound.success.source.sound", "Stopped sound '%s' on source '%s'"},
				{"commands.stopsound.success.sourceless.any", "Stopped all sounds"},
				{"commands.stopsound.success.sourceless.sound", "Stopped sound '%s'"},
				{"commands.transfer.success.multiple", "Transferring %s players to %s:%s"},
				{"commands.transfer.success.single", "Transferring %s to %s:%s"},
				{"arguments.operation.div0", "Cannot divide by zero"},
				{"arguments.operation.invalid", "Invalid operation"},
				{"commands.scoreboard.players.enable.invalid", "Enable only works on trigger-objectives"},
				{"commands.scoreboard.players.enable.success.single", "Enabled trigger %s for %s"},
				{"commands.scoreboard.players.operation.success.multiple", "Updated %s for %s entities"},
				{"commands.scoreboard.players.operation.success.single", "Set %s for %s to %s"},
				{"commands.trigger.add.success", "Triggered %s (added %s to value)"},
				{"commands.trigger.failed.invalid", "You can only trigger objectives that are 'trigger' type"},
				{"commands.trigger.failed.unprimed", "You cannot trigger this objective yet"},
				{"commands.trigger.set.success", "Triggered %s (set value to %s)"},
				{"commands.trigger.simple.success", "Triggered %s"},
				{"commands.title.cleared.multiple", "Cleared titles for %s players"},
				{"commands.title.cleared.single", "Cleared titles for %s"},
				{"commands.title.reset.multiple", "Reset title options for %s players"},
				{"commands.title.reset.single", "Reset title options for %s"},
				{"commands.title.show.actionbar.multiple", "Showing new actionbar title for %s players"},
				{"commands.title.show.actionbar.single", "Showing new actionbar title for %s"},
				{"commands.title.show.subtitle.multiple", "Showing new subtitle for %s players"},
				{"commands.title.show.subtitle.single", "Showing new subtitle for %s"},
				{"commands.title.show.title.multiple", "Showing new title for %s players"},
				{"commands.title.show.title.single", "Showing new title for %s"},
				{"commands.title.times.multiple", "Changed title display times for %s players"},
				{"commands.title.times.single", "Changed title display times for %s"},
				{"commands.unknown.command", "Unknown or incomplete command, see below for error"},
				{"commands.worldborder.center.failed", "Nothing changed. The world border is already centered there"},
				{"commands.worldborder.center.success", "Set the center of the world border to %s, %s"},
				{"commands.worldborder.damage.amount.failed", "Nothing changed. The world border damage is already that amount"},
				{"commands.worldborder.damage.amount.success", "Set the world border damage to %s per block each second"},
				{"commands.worldborder.damage.buffer.failed", "Nothing changed. The world border damage buffer is already that distance"},
				{"commands.worldborder.damage.buffer.success", "Set the world border damage buffer to %s block(s)"},
				{"commands.worldborder.get", "The world border is currently %s block(s) wide"},
				{"commands.worldborder.set.failed.big", "World border cannot be bigger than %s blocks wide"},
				{"commands.worldborder.set.failed.nochange", "Nothing changed. The world border is already that size"},
				{"commands.worldborder.set.failed.small", "World border cannot be smaller than 1 block wide"},
				{"commands.worldborder.set.grow", "Growing the world border to %s blocks wide over %s seconds"},
				{"commands.worldborder.set.immediate", "Set the world border to %s block(s) wide"},
				{"commands.worldborder.set.shrink", "Shrinking the world border to %s block(s) wide over %s second(s)"},
				{"commands.worldborder.warning.distance.failed", "Nothing changed. The world border warning is already that distance"},
				{"commands.worldborder.warning.distance.success", "Set the world border warning distance to %s block(s)"},
				{"commands.worldborder.warning.time.failed", "Nothing changed. The world border warning is already that amount of time"},
				{"commands.worldborder.warning.time.success", "Set the world border warning time to %s second(s)"},
				{"commands.weather.set.clear", "Set the weather to clear"},
				{"commands.weather.set.rain", "Set the weather to rain"},
				{"commands.weather.set.thunder", "Set the weather to rain & thunder"},
				{"commands.whitelist.add.failed", "Player is already whitelisted"},
				{"commands.whitelist.add.success", "Added %s to the whitelist"},
				{"commands.whitelist.alreadyOff", "Whitelist is already turned off"},
				{"commands.whitelist.alreadyOn", "Whitelist is already turned on"},
				{"commands.whitelist.disabled", "Whitelist is now turned off"},
				{"commands.whitelist.enabled", "Whitelist is now turned on"},
				{"commands.whitelist.list", "There are %s whitelisted player(s): %s"},
				{"commands.whitelist.none", "There are no whitelisted players"},
				{"commands.whitelist.reloaded", "Reloaded the whitelist"},
				{"commands.whitelist.remove.failed", "Player is not whitelisted"},
				{"commands.whitelist.remove.success", "Removed %s from the whitelist"},
				{"multiplayer.disconnect.kicked", "Kicked by an operator"},
				{"permissions.requires.player", "A player is required to run this command here"},
			};
			return TEXT;
		}
	} // namespace

	std::string englishText(const std::string& key, const std::vector<std::string>& args) {
		auto it = english().find(key);
		if (it == english().end()) {
			std::string text = key;
			for (const std::string& arg : args) text += " " + arg;
			return text;
		}
		// String.format's %s (the next argument) and %1$s (a numbered one)
		const std::string& pattern = it->second;
		std::string		   text;
		size_t			   next = 0;
		for (size_t i = 0; i < pattern.size(); i++) {
			if (pattern[i] != '%' || i + 1 >= pattern.size()) {
				text += pattern[i];
			} else if (pattern[i + 1] == 's') {
				if (next < args.size()) text += args[next];
				next++;
				i++;
			} else if (std::isdigit(static_cast<unsigned char>(pattern[i + 1])) && pattern.compare(i + 2, 2, "$s") == 0) {
				size_t index = static_cast<size_t>(pattern[i + 1] - '1');
				if (index < args.size()) text += args[index];
				i += 3;
			} else {
				text += pattern[i];
			}
		}
		return text;
	}
} // namespace Commands
