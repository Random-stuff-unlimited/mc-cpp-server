#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"

#include <memory>
#include <string>
#include <vector>

// The command tree the client gets (ClientboundCommandsPacket): what it suggests and colors as the player types.
// Brigadier's nodes, flattened: the root, literals ("tp", "set") and arguments with their parser
// (minecraft:command_argument_type) and its properties. Each command's syntax here follows what its run function
// accepts
namespace {
	constexpr uint8_t TYPE_ROOT = 0, TYPE_LITERAL = 1, TYPE_ARGUMENT = 2;
	constexpr uint8_t FLAG_EXECUTABLE = 4, FLAG_CUSTOM_SUGGESTIONS = 16;
	// EntityArgument's flags: a single entity, players only
	constexpr uint8_t ENTITY_SINGLE = 1, ENTITY_PLAYERS_ONLY = 2;
	// StringArgumentType: SINGLE_WORD, QUOTABLE_PHRASE, GREEDY_PHRASE
	constexpr int STRING_WORD = 0;

	struct Node {
		uint8_t			 type;
		std::string		 name;
		bool			 executable = false;
		std::vector<int> children;
		int				 parser = -1;
		Buffer			 properties;
		std::string		 suggestions; // "minecraft:ask_server"... empty: the parser's own
	};

	class Tree {
	  public:
		explicit Tree(const GameData& data) : _data(data) { _nodes.push_back(std::make_unique<Node>(Node{TYPE_ROOT, "", false, {}, -1, {}, {}})); }

		int literal(const std::string& name, bool executable, std::vector<int> children = {}) {
			return add(Node{TYPE_LITERAL, name, executable, std::move(children), -1, {}, {}});
		}
		// An argument with its parser ("minecraft:vec3") and the parser's properties
		int argument(const std::string& name, const std::string& parser, bool executable, std::vector<int> children = {}, Buffer properties = {}) {
			int id = _data.getStaticId("minecraft:command_argument_type", parser);
			return add(Node{TYPE_ARGUMENT, name, executable, std::move(children), id, std::move(properties), {}});
		}
		int players(const std::string& name, bool single, bool executable, std::vector<int> children = {}) {
			Buffer flags;
			flags.writeUByte(static_cast<uint8_t>((single ? ENTITY_SINGLE : 0) | ENTITY_PLAYERS_ONLY));
			return argument(name, "minecraft:entity", executable, std::move(children), std::move(flags));
		}
		int integer(const std::string& name, int min, bool executable, std::vector<int> children = {}) {
			Buffer props;
			props.writeUByte(1); // Min only (ArgumentUtils.createNumberFlags)
			props.writeInt(min);
			return argument(name, "brigadier:integer", executable, std::move(children), std::move(props));
		}
		int word(const std::string& name, bool executable, std::vector<int> children = {}) {
			Buffer props;
			props.writeVarInt(STRING_WORD);
			return argument(name, "brigadier:string", executable, std::move(children), std::move(props));
		}
		// The client asks the server what to suggest for this argument (SuggestionProviders.ASK_SERVER)
		int askServer(int node) {
			_nodes[node]->suggestions = "minecraft:ask_server";
			return node;
		}
		int time(const std::string& name, bool executable) {
			Buffer props;
			props.writeInt(0); // TimeArgument's minimum
			return argument(name, "minecraft:time", executable, {}, std::move(props));
		}
		// Literals that each end the command
		std::vector<int> choices(std::initializer_list<const char*> names) {
			std::vector<int> ids;
			for (const char* name : names) ids.push_back(literal(name, true));
			return ids;
		}
		void addCommand(int node) { _nodes[0]->children.push_back(node); }

		void write(Buffer& buf) const {
			buf.writeVarInt(static_cast<int32_t>(_nodes.size()));
			for (const auto& node : _nodes) {
				uint8_t flags = node->type;
				if (node->executable) flags |= FLAG_EXECUTABLE;
				if (!node->suggestions.empty()) flags |= FLAG_CUSTOM_SUGGESTIONS;
				buf.writeUByte(flags);
				buf.writeVarInt(static_cast<int32_t>(node->children.size()));
				for (int child : node->children) buf.writeVarInt(child);
				if (node->type == TYPE_ROOT) continue;
				buf.writeString(node->name);
				if (node->type == TYPE_ARGUMENT) {
					buf.writeVarInt(node->parser);
					buf.writeBytes(node->properties.getData());
					if (!node->suggestions.empty()) buf.writeString(node->suggestions);
				}
			}
			buf.writeVarInt(0); // The root's index
		}

	  private:
		const GameData&					   _data;
		std::vector<std::unique_ptr<Node>> _nodes;

		int add(Node node) {
			_nodes.push_back(std::make_unique<Node>(std::move(node)));
			return static_cast<int>(_nodes.size()) - 1;
		}
	};

	// The syntax of each built-in command, by name (the ones without an entry are sent as a bare literal)
	int commandNode(Tree& t, const std::string& name) {
		if (name == "tp") {
			return t.literal("tp", false,
							 {t.argument("location", "minecraft:vec3", true),
							  t.players("target", true, false, {t.argument("location", "minecraft:vec3", true)})});
		}
		if (name == "gamemode" || name == "gm") {
			return t.literal(name, false, {t.argument("gamemode", "minecraft:gamemode", true, {t.players("target", true, true)})});
		}
		if (name == "spawn" || name == "list") return t.literal(name, true);
		if (name == "setworldspawn" || name == "spawnpoint") return t.literal(name, true, {t.argument("pos", "minecraft:block_pos", true)});
		if (name == "execute") {
			int tp = t.literal("tp", true, {t.argument("location", "minecraft:vec3", true)});
			return t.literal("execute", false, {t.literal("in", false, {t.argument("dimension", "minecraft:dimension", false, {t.literal("run", false, {tp})})})});
		}
		if (name == "fill") {
			std::vector<int> modes = t.choices({"destroy", "hollow", "keep", "outline", "strict"});
			modes.push_back(t.literal("replace", true, {t.argument("filter", "minecraft:block_predicate", true)}));
			int block = t.argument("block", "minecraft:block_state", true, modes);
			return t.literal("fill", false, {t.argument("from", "minecraft:block_pos", false, {t.argument("to", "minecraft:block_pos", false, {block})})});
		}
		if (name == "setblock") {
			int block = t.argument("block", "minecraft:block_state", true, t.choices({"destroy", "keep", "replace"}));
			return t.literal("setblock", false, {t.argument("pos", "minecraft:block_pos", false, {block})});
		}
		if (name == "time") {
			std::vector<int> set = t.choices({"day", "noon", "night", "midnight"});
			set.push_back(t.time("time", true));
			return t.literal("time", false,
							 {t.literal("set", false, set), t.literal("add", false, {t.time("time", true)}),
							  t.literal("query", false, t.choices({"daytime", "gametime"}))});
		}
		if (name == "give") {
			int item = t.argument("item", "minecraft:item_stack", true, {t.integer("count", 1, true)});
			return t.literal("give", false, {t.players("targets", false, false, {item})});
		}
		if (name == "clear" || name == "kill") return t.literal(name, true, {t.players("targets", false, true)});
		if (name == "difficulty") return t.literal("difficulty", true, t.choices({"peaceful", "easy", "normal", "hard"}));
		if (name == "weather") return t.literal("weather", false, t.choices({"clear", "rain", "thunder"}));
		if (name == "effect") {
			Buffer registry;
			registry.writeString("minecraft:mob_effect"); // ResourceArgument's registry
			int hide	  = t.argument("hideParticles", "brigadier:bool", true);
			int amplifier = t.argument("amplifier", "brigadier:integer", true, {hide}, [] {
				Buffer props;
				props.writeUByte(3); // Min and max
				props.writeInt(0);
				props.writeInt(255);
				return props;
			}());
			int seconds = t.argument("seconds", "brigadier:integer", true, {amplifier}, [] {
				Buffer props;
				props.writeUByte(3);
				props.writeInt(1);
				props.writeInt(1000000);
				return props;
			}());
			int infinite = t.literal("infinite", true, {t.argument("amplifier", "brigadier:integer", true, {t.argument("hideParticles", "brigadier:bool", true)}, [] {
				Buffer props;
				props.writeUByte(3);
				props.writeInt(0);
				props.writeInt(255);
				return props;
			}())});
			Buffer registry2;
			registry2.writeString("minecraft:mob_effect");
			int give  = t.literal("give", false, {t.players("targets", false, false, {t.argument("effect", "minecraft:resource", true, {seconds, infinite}, std::move(registry))})});
			int clear = t.literal("clear", true, {t.players("targets", false, true, {t.argument("effect", "minecraft:resource", true, {}, std::move(registry2))})});
			return t.literal("effect", false, {give, clear});
		}
		if (name == "summon") {
			Buffer registry;
			registry.writeString("minecraft:entity_type");
			return t.literal("summon", false, {t.argument("entity", "minecraft:resource", true, {t.argument("pos", "minecraft:vec3", true)}, std::move(registry))});
		}
		if (name == "xp" || name == "experience") {
			auto amount = [&] { return t.argument("amount", "brigadier:integer", true, t.choices({"points", "levels"})); };
			int	 add	= t.literal("add", false, {t.players("targets", false, false, {amount()})});
			int	 set	= t.literal("set", false, {t.players("targets", false, false, {amount()})});
			int	 query	= t.literal("query", false, {t.players("target", true, false, t.choices({"points", "levels"}))});
			return t.literal(name, false, {add, set, query});
		}
		if (name == "enchant") {
			Buffer registry;
			registry.writeString("minecraft:enchantment");
			int level = t.integer("level", 1, true);
			return t.literal("enchant", false, {t.players("targets", false, false, {t.argument("enchantment", "minecraft:resource", true, {level}, std::move(registry))})});
		}
		if (name == "help") return t.literal("help", true, {t.askServer(t.word("command", true))});
		// The admin commands: player names (GameProfileArgument), a free reason (MessageArgument)
		auto message = [&](const char* arg) { return t.argument(arg, "minecraft:message", true); };
		auto profile = [&](const char* arg, bool executable, std::vector<int> children = {}) {
			return t.argument(arg, "minecraft:game_profile", executable, std::move(children));
		};
		if (name == "op" || name == "deop" || name == "pardon") return t.literal(name, false, {profile("targets", true)});
		if (name == "ban") return t.literal("ban", false, {profile("targets", true, {message("reason")})});
		if (name == "kick") return t.literal("kick", false, {t.players("targets", false, true, {message("reason")})});
		if (name == "ban-ip") return t.literal("ban-ip", false, {t.word("target", true, {message("reason")})});
		if (name == "pardon-ip") return t.literal("pardon-ip", false, {t.word("target", true)});
		if (name == "banlist") return t.literal("banlist", true, t.choices({"ips", "players"}));
		if (name == "whitelist") {
			std::vector<int> subs = t.choices({"on", "off", "list", "reload"});
			subs.push_back(t.literal("add", false, {profile("targets", true)}));
			subs.push_back(t.literal("remove", false, {profile("targets", true)}));
			return t.literal("whitelist", false, subs);
		}
		if (name == "title") {
			std::vector<int> actions = t.choices({"clear", "reset"});
			for (const char* kind : {"title", "subtitle", "actionbar"}) actions.push_back(t.literal(kind, false, {message(kind)}));
			int fadeOut = t.integer("fadeOut", 0, true);
			actions.push_back(t.literal("times", false, {t.integer("fadeIn", 0, false, {t.integer("stay", 0, false, {fadeOut})})}));
			return t.literal("title", false, {t.players("targets", false, false, actions)});
		}
		if (name == "worldborder") {
			auto num = [&](const char* arg, bool exec, std::vector<int> children = {}) { return t.argument(arg, "brigadier:double", exec, std::move(children), [] {
				Buffer props;
				props.writeUByte(0); // No bounds
				return props;
			}()); };
			std::vector<int> subs = t.choices({"get"});
			subs.push_back(t.literal("set", false, {num("distance", true, {t.integer("time", 0, true)})}));
			subs.push_back(t.literal("add", false, {num("distance", true, {t.integer("time", 0, true)})}));
			subs.push_back(t.literal("center", false, {t.argument("pos", "minecraft:vec2", true)}));
			subs.push_back(t.literal("damage", false, {t.literal("amount", false, {num("damagePerBlock", true)}), t.literal("buffer", false, {num("distance", true)})}));
			subs.push_back(t.literal("warning", false, {t.literal("distance", false, {t.integer("distance", 0, true)}), t.literal("time", false, {t.integer("time", 0, true)})}));
			return t.literal("worldborder", false, subs);
		}
		if (name == "scoreboard") {
			// The arguments are words and greedy text here: the client's own parsers for objectives and criteria would
			// need the server's lists
			auto rest = [&](const char* arg) { return message(arg); };
			int	 objList = t.literal("list", true);
			int	 objAdd	 = t.literal("add", false, {t.word("objective", false, {t.word("criteria", true, {rest("displayName")})})});
			int	 objRem	 = t.literal("remove", false, {t.word("objective", true)});
			int	 objDisp = t.literal("setdisplay", false, {t.word("slot", true, {t.word("objective", true)})});
			int	 objMod	 = t.literal("modify", false, {t.word("objective", false, {t.literal("displayname", false, {rest("displayName")}),
																					 t.literal("rendertype", false, t.choices({"hearts", "integer"}))})});
			auto withScore = [&](const char* sub) { return t.literal(sub, false, {t.word("targets", false, {t.word("objective", false, {t.integer("score", -2147483647 - 1, true)})})}); };
			int	 plList	 = t.literal("list", true, {t.word("target", true)});
			int	 plGet	 = t.literal("get", false, {t.word("target", false, {t.word("objective", true)})});
			int	 plReset = t.literal("reset", false, {t.word("targets", true, {t.word("objective", true)})});
			return t.literal("scoreboard", false, {t.literal("objectives", false, {objList, objAdd, objRem, objDisp, objMod}),
												   t.literal("players", false, {plList, plGet, withScore("set"), withScore("add"), withScore("remove"), plReset})});
		}
		if (name == "save-all") return t.literal("save-all", true, t.choices({"flush"}));
		if (name == "say" || name == "me") return t.literal(name, false, {message(name == "say" ? "message" : "action")});
		if (name == "tell" || name == "msg" || name == "w") return t.literal(name, false, {t.players("targets", false, false, {message("message")})});
		return t.literal(name, true);
	}
} // namespace

namespace Commands {
	void sendCommandTree(Server& server, Player& player) {
		Tree tree(server.getGameData());
		for (const Command& command : commandList()) {
			if (canUse(player, command)) tree.addCommand(commandNode(tree, command.name));
		}
		Buffer buf;
		tree.write(buf);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::COMMANDS, buf, server);
	}

	void suggest(Server& server, Player& player, int transaction, const std::string& text) {
		// The arguments asking the server: help's command name
		size_t					 start = text.rfind(' ') + 1; // The word being typed (0 if none: the whole text)
		std::string				 word  = text.substr(start);
		std::vector<std::string> matches;
		if (text.rfind("/help ", 0) == 0 && start == 6) {
			for (const Command& command : commandList()) {
				if (canUse(player, command) && command.name.rfind(word, 0) == 0) matches.push_back(command.name);
			}
		}
		// ClientboundCommandSuggestionsPacket: the range replaced (in the text as typed, '/' included), then the entries
		Buffer buf;
		buf.writeVarInt(transaction);
		buf.writeVarInt(static_cast<int32_t>(start));
		buf.writeVarInt(static_cast<int32_t>(word.size()));
		buf.writeVarInt(static_cast<int32_t>(matches.size()));
		for (const std::string& match : matches) {
			buf.writeString(match);
			buf.writeBool(false); // No tooltip
		}
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::COMMAND_SUGGESTIONS, buf, server);
	}
} // namespace Commands
