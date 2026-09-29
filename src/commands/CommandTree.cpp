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
		if (name == "help") return t.literal("help", true, {t.askServer(t.word("command", true))});
		return t.literal(name, true);
	}
} // namespace

namespace Commands {
	void sendCommandTree(Server& server, Player& player) {
		Tree tree(server.getGameData());
		for (const Command& command : commandList()) {
			if (command.run) tree.addCommand(commandNode(tree, command.name));
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
				if (command.run && command.name.rfind(word, 0) == 0) matches.push_back(command.name);
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
