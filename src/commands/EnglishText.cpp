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
