#include "Commands.hpp"

#include "PacketIds.hpp"
#include "commands/CommandSupport.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"

#include <cmath>
#include <cstdlib>

// TransferCommand, PlaySoundCommand, StopSoundCommand, RotateCommand
namespace Commands {
	namespace {
		const char* const SOURCES[] = {"master", "music", "record", "weather", "block", "hostile", "neutral", "player", "ambient", "voice", "ui"};

		int sourceByName(const std::string& name) {
			for (int i = 0; i < 11; i++) {
				if (name == SOURCES[i]) return i;
			}
			return -1;
		}

		bool number(const std::string& text, double& out) {
			char* end = nullptr;
			out		  = std::strtod(text.c_str(), &end);
			return end != text.c_str() && *end == '\0' && std::isfinite(out);
		}

		std::string withNamespace(const std::string& name) { return name.find(':') == std::string::npos ? "minecraft:" + name : name; }

		void transfer(Server& server, Player& player, const std::vector<std::string>& args) {
			const std::string syntax = "transfer <hostname> [<port>] [<players>]";
			if (args.empty() || args.size() > 3) return usage(server, player, syntax);
			double port = 25565;
			if (args.size() >= 2 && (!number(args[1], port) || port < 1 || port > 65535 || port != std::floor(port))) return usage(server, player, syntax);
			std::vector<Player*> targets = args.size() == 3 ? playerTargets(server, player, args[2]) : std::vector<Player*>{&player};
			if (player.isConsole() && args.size() < 3) return failure(server, player, "permissions.requires.player");
			Buffer data;
			data.writeString(args[0]);
			data.writeVarInt(static_cast<int>(port));
			for (Player* target : targets) Packet::send(target->shared_from_this(), PacketId::Play::Clientbound::TRANSFER, data, server);
			if (targets.size() == 1) return translated(server, player, "commands.transfer.success.single", {targets[0]->getPlayerName(), args[0], std::to_string(static_cast<int>(port))});
			translated(server, player, "commands.transfer.success.multiple", {std::to_string(targets.size()), args[0], std::to_string(static_cast<int>(port))});
		}

		// /playsound <sound> [<source>] [<targets>] [<x> <y> <z>] [<volume>] [<pitch>]
		void playsound(Server& server, Player& player, const std::vector<std::string>& args) {
			const std::string syntax = "playsound <sound> [<source>] [<targets>] [<x> <y> <z>] [<volume>] [<pitch>]";
			if (args.empty()) return usage(server, player, syntax);
			std::string sound  = withNamespace(args[0]);
			int			source = args.size() >= 2 ? sourceByName(args[1]) : 0;
			if (source < 0) return usage(server, player, syntax);
			std::vector<Player*> targets = args.size() >= 3 ? playerTargets(server, player, args[2]) : std::vector<Player*>{&player};
			if (targets.empty()) return failure(server, player, "argument.entity.notfound.player");
			double volume = 1.0, pitch = 1.0;
			if (args.size() >= 7 && !number(args[6], volume)) return usage(server, player, syntax);
			if (args.size() >= 8 && !number(args[7], pitch)) return usage(server, player, syntax);
			for (Player* target : targets) {
				double x = target->getX(), y = target->getY(), z = target->getZ();
				if (args.size() >= 6 && (!parseCoord(player.getX(), args[3], x) || !parseCoord(player.getY(), args[4], y) || !parseCoord(player.getZ(), args[5], z))) {
					return usage(server, player, syntax);
				}
				// ClientboundSoundPacket: the event inline (its name) when it isn't a registered one
				Buffer data;
				int	   id = server.levelOf(*target).gameData().getStaticId("minecraft:sound_event", sound);
				if (id >= 0) {
					data.writeVarInt(id + 1);
				} else {
					data.writeVarInt(0);
					data.writeString(sound);
					data.writeBool(false); // No fixed range
				}
				data.writeVarInt(source);
				data.writeInt(static_cast<int32_t>(x * 8.0));
				data.writeInt(static_cast<int32_t>(y * 8.0));
				data.writeInt(static_cast<int32_t>(z * 8.0));
				data.writeFloat(static_cast<float>(volume));
				data.writeFloat(static_cast<float>(std::clamp(pitch, 0.0, 2.0)));
				data.writeLong(static_cast<int64_t>(std::rand()));
				Packet::send(target->shared_from_this(), PacketId::Play::Clientbound::SOUND, data, server);
			}
			if (targets.size() == 1) return translated(server, player, "commands.playsound.success.single", {sound, targets[0]->getPlayerName()});
			translated(server, player, "commands.playsound.success.multiple", {sound, std::to_string(targets.size())});
		}

		// /stopsound <targets> [<source>|*] [<sound>]
		void stopsound(Server& server, Player& player, const std::vector<std::string>& args) {
			const std::string syntax = "stopsound <targets> [<source>|*] [<sound>]";
			if (args.empty() || args.size() > 3) return usage(server, player, syntax);
			std::vector<Player*> targets = playerTargets(server, player, args[0]);
			if (targets.empty()) return failure(server, player, "argument.entity.notfound.player");
			int source = -1;
			if (args.size() >= 2 && args[1] != "*") {
				source = sourceByName(args[1]);
				if (source < 0) return usage(server, player, syntax);
			}
			std::string sound = args.size() == 3 ? withNamespace(args[2]) : "";
			// ClientboundStopSoundPacket: flags (1 a source, 2 a sound), then each
			Buffer data;
			data.writeByte(static_cast<int8_t>((source >= 0 ? 1 : 0) | (sound.empty() ? 0 : 2)));
			if (source >= 0) data.writeVarInt(source);
			if (!sound.empty()) data.writeString(sound);
			for (Player* target : targets) Packet::send(target->shared_from_this(), PacketId::Play::Clientbound::STOP_SOUND, data, server);
			if (sound.empty() && source < 0) return translated(server, player, "commands.stopsound.success.sourceless.any");
			if (sound.empty()) return translated(server, player, "commands.stopsound.success.source.any", {args[1]});
			translated(server, player, source >= 0 ? "commands.stopsound.success.source.sound" : "commands.stopsound.success.sourceless.sound",
					   source >= 0 ? std::vector<std::string>{sound, args[1]} : std::vector<std::string>{sound});
		}

		// /rotate <target> (<yaw> <pitch> | facing <x> <y> <z>)
		void rotate(Server& server, Player& player, const std::vector<std::string>& args) {
			const std::string syntax = "rotate <target> (<yaw> <pitch>|facing <x> <y> <z>)";
			if (args.size() != 3 && args.size() != 5) return usage(server, player, syntax);
			std::vector<Player*> targets = playerTargets(server, player, args[0]);
			if (targets.size() != 1) return failure(server, player, "argument.entity.notfound.player");
			Player& target = *targets[0];
			if (args.size() == 5 && args[1] == "facing") {
				double x, y, z;
				if (!parseCoord(player.getX(), args[2], x) || !parseCoord(player.getY(), args[3], y) || !parseCoord(player.getZ(), args[4], z)) {
					return usage(server, player, syntax);
				}
				double dx = x - target.getX(), dy = y - (target.getY() + 1.62), dz = z - target.getZ();
				float  yaw	 = static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI) - 90.0F;
				float  pitch = static_cast<float>(-(std::atan2(dy, std::sqrt(dx * dx + dz * dz)) * 180.0 / M_PI));
				target.setRotation(yaw, pitch);
				// ClientboundPlayerLookAtPacket: from its eyes toward the point
				Buffer data;
				data.writeVarInt(1); // EntityAnchorArgument.Anchor.EYES
				data.writeDouble(x);
				data.writeDouble(y);
				data.writeDouble(z);
				data.writeBool(false); // Not at an entity
				Packet::send(target.shared_from_this(), PacketId::Play::Clientbound::PLAYER_LOOK_AT, data, server);
			} else if (args.size() == 3) {
				double yaw, pitch;
				if (!parseCoord(target.getYaw(), args[1], yaw) || !parseCoord(target.getPitch(), args[2], pitch)) return usage(server, player, syntax);
				target.setRotation(static_cast<float>(yaw), static_cast<float>(std::clamp(pitch, -90.0, 90.0)));
				teleport(server, target, target.getX(), target.getY(), target.getZ()); // The client takes the rotation with its position
			} else {
				return usage(server, player, syntax);
			}
			translated(server, player, "commands.rotate.success", {target.getPlayerName()});
		}
	} // namespace

	void registerSoundCommands() {
		commandList().push_back({"transfer", "transfer <hostname> [<port>] [<players>]", transfer, LEVEL_ADMINS});
		commandList().push_back({"playsound", "playsound <sound> [<source>] [<targets>] [<x> <y> <z>] [<volume>] [<pitch>]", playsound});
		commandList().push_back({"stopsound", "stopsound <targets> [<source>|*] [<sound>]", stopsound});
		commandList().push_back({"rotate", "rotate <target> (<yaw> <pitch>|facing <x> <y> <z>)", rotate});
	}
} // namespace Commands
