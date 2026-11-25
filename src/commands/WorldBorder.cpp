#include "Commands.hpp"

#include "PacketIds.hpp"
#include "commands/CommandSupport.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// WorldBorderCommand: get, set and add (over some seconds), center, damage amount / buffer, warning distance / time.
// Every player sees the change (the border is the overworld's, for every dimension)
namespace Commands {
	namespace {
		const std::string SYNTAX = "worldborder (get|set <distance> [<time>]|add <distance> [<time>]|center <x> <z>|damage (amount|buffer) <value>|"
								   "warning (distance|time) <value>)";

		bool number(const std::string& text, double& out) {
			char* end = nullptr;
			out		  = std::strtod(text.c_str(), &end);
			return end != text.c_str() && *end == '\0' && std::isfinite(out);
		}
		std::string format(double value) {
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", value);
			return buf;
		}

		void broadcast(Server& server, int packetId, Buffer& data) {
			for (const auto& player : server.getGamePlayers()) Packet::send(player, packetId, data, server);
		}

		// WorldBorderCommand.setSize: now, or over seconds
		void setSize(Server& server, Player& player, Level& level, WorldBorder& border, double size, int64_t seconds) {
			int64_t now	   = level.getGameTime();
			double	before = border.size(now);
			if (before == size && border.lerpRemaining(now) == 0) return failure(server, player, "commands.worldborder.set.failed.nochange");
			if (size < 1.0) return failure(server, player, "commands.worldborder.set.failed.small");
			if (size > WorldBorder::MAX_SIZE) return failure(server, player, "commands.worldborder.set.failed.big", {format(WorldBorder::MAX_SIZE)});
			if (seconds > 0) {
				border.lerpSizeBetween(before, size, seconds * 20, now);
				Buffer data;
				data.writeDouble(before);
				data.writeDouble(size);
				data.writeVarLong(seconds * 1000);
				broadcast(server, PacketId::Play::Clientbound::SET_BORDER_LERP_SIZE, data);
				translated(server, player, size > before ? "commands.worldborder.set.grow" : "commands.worldborder.set.shrink",
						   {format(size), std::to_string(seconds)});
			} else {
				border.setSize(size);
				Buffer data;
				data.writeDouble(size);
				broadcast(server, PacketId::Play::Clientbound::SET_BORDER_SIZE, data);
				translated(server, player, "commands.worldborder.set.immediate", {format(size)});
			}
			level.world().saveBorder();
		}

		void runWorldBorder(Server& server, Player& player, const std::vector<std::string>& args) {
			if (args.empty()) return usage(server, player, SYNTAX);
			Level&		 level	= server.levelOf(player);
			WorldBorder& border = level.world().border();
			int64_t		 now	= level.getGameTime();
			const std::string& sub = args[0];
			double			   a = 0, b = 0;
			if (sub == "get" && args.size() == 1) {
				return translated(server, player, "commands.worldborder.get", {std::to_string(static_cast<int64_t>(border.size(now) + 0.5))});
			}
			if ((sub == "set" || sub == "add") && (args.size() == 2 || args.size() == 3) && number(args[1], a)) {
				int64_t seconds = 0;
				if (args.size() == 3) {
					if (!number(args[2], b) || b < 0) return usage(server, player, SYNTAX);
					seconds = static_cast<int64_t>(b);
				}
				// add grows from where the lerp is heading (WorldBorderCommand: getLerpTarget)
				double size = sub == "add" ? (border.lerpRemaining(now) > 0 ? border.toSize : border.size(now)) + a : a;
				return setSize(server, player, level, border, size, seconds);
			}
			if (sub == "center" && args.size() == 3 && number(args[1], a) && number(args[2], b)) {
				if (std::abs(a) > 2.9999984E7 || std::abs(b) > 2.9999984E7) return failure(server, player, "commands.worldborder.center.failed");
				if (border.centerX == a && border.centerZ == b) return failure(server, player, "commands.worldborder.center.failed");
				border.centerX = a;
				border.centerZ = b;
				Buffer data;
				data.writeDouble(a);
				data.writeDouble(b);
				broadcast(server, PacketId::Play::Clientbound::SET_BORDER_CENTER, data);
				level.world().saveBorder();
				return translated(server, player, "commands.worldborder.center.success", {format(a), format(b)});
			}
			if (sub == "damage" && args.size() == 3 && number(args[2], a) && a >= 0) {
				if (args[1] == "amount") {
					if (border.damagePerBlock == a) return failure(server, player, "commands.worldborder.damage.amount.failed");
					border.damagePerBlock = a;
					level.world().saveBorder();
					return translated(server, player, "commands.worldborder.damage.amount.success", {format(a)});
				}
				if (args[1] == "buffer") {
					if (border.safeZone == a) return failure(server, player, "commands.worldborder.damage.buffer.failed");
					border.safeZone = a;
					level.world().saveBorder();
					return translated(server, player, "commands.worldborder.damage.buffer.success", {format(a)});
				}
			}
			if (sub == "warning" && args.size() == 3 && number(args[2], a) && a >= 0 && a == std::floor(a)) {
				int value = static_cast<int>(a);
				if (args[1] == "distance") {
					if (border.warningBlocks == value) return failure(server, player, "commands.worldborder.warning.distance.failed");
					border.warningBlocks = value;
					Buffer data;
					data.writeVarInt(value);
					broadcast(server, PacketId::Play::Clientbound::SET_BORDER_WARNING_DISTANCE, data);
					level.world().saveBorder();
					return translated(server, player, "commands.worldborder.warning.distance.success", {std::to_string(value)});
				}
				if (args[1] == "time") {
					if (border.warningTime == value) return failure(server, player, "commands.worldborder.warning.time.failed");
					border.warningTime = value;
					Buffer data;
					data.writeVarInt(value);
					broadcast(server, PacketId::Play::Clientbound::SET_BORDER_WARNING_DELAY, data);
					level.world().saveBorder();
					return translated(server, player, "commands.worldborder.warning.time.success", {std::to_string(value)});
				}
			}
			usage(server, player, SYNTAX);
		}
	} // namespace

	void registerWorldBorder() { commandList().push_back({"worldborder", SYNTAX, runWorldBorder}); }
} // namespace Commands
