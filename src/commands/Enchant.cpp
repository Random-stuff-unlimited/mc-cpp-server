#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/Enchantments.hpp"

#include <cstdlib>

// EnchantCommand: /enchant <targets> <enchantment> [<level>]: the item in each target's hand, if the enchantment
// supports it, its level is at most the maximum, and it is compatible with those it has
namespace {
	bool supports(const GameData& data, const nlohmann::json& definition, int item) {
		if (!definition.contains("supported_items")) return false;
		const nlohmann::json& items = definition.at("supported_items");
		auto matches = [&](const std::string& name) {
			return name[0] == '#' ? data.isInTag("minecraft:item", name.substr(1), item) : data.getStaticId("minecraft:item", name) == item;
		};
		if (items.is_array()) {
			for (const auto& entry : items) {
				if (matches(entry.get<std::string>())) return true;
			}
			return false;
		}
		return matches(items.get<std::string>());
	}
} // namespace

namespace Commands {
	void registerEnchant() {
		commandList().push_back({"enchant", "enchant <targets> <enchantment> [<level>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
									 if (args.size() < 2 || args.size() > 3) {
										 usage(server, player, "enchant <targets> <enchantment> [<level>]");
										 return;
									 }
									 const GameData& data = server.levelOf(player).gameData();
									 std::string		 name = args[1].find(':') == std::string::npos ? "minecraft:" + args[1] : args[1];
									 int				 id	  = data.getSyncedId("minecraft:enchantment", name);
									 const nlohmann::json* definition = id >= 0 ? Enchantments::definition(data, id) : nullptr;
									 if (!definition) {
										 message(server, player, "Unknown enchantment: " + name);
										 return;
									 }
									 int level = 1;
									 if (args.size() == 3) {
										 char* end = nullptr;
										 level	   = static_cast<int>(std::strtol(args[2].c_str(), &end, 10));
										 if (end == args[2].c_str() || *end != '\0' || level < 1) {
											 usage(server, player, "enchant <targets> <enchantment> [<level>]");
											 return;
										 }
									 }
									 int max = definition->value("max_level", 1);
									 if (level > max) {
										 message(server, player, std::to_string(level) + " is higher than the maximum level of " + std::to_string(max) + " supported by that enchantment");
										 return;
									 }
									 int done = 0;
									 for (Player* target : playerTargets(server, player, args[0])) {
										 ItemStack& stack = target->inventory().getMutable(target->handSlot(0));
										 if (stack.isEmpty() || !supports(data, *definition, stack.item)) {
											 message(server, player, target->getPlayerName() + "'s item can't be enchanted with that");
											 continue;
										 }
										 std::vector<std::pair<int, int>> current = Enchantments::of(data, stack);
										 bool							  compatible = true;
										 for (auto [other, otherLevel] : current) {
											 if (other != id && !Enchantments::areCompatible(data, id, other)) compatible = false;
										 }
										 if (!compatible) {
											 message(server, player, target->getPlayerName() + "'s item has an enchantment that conflicts with it");
											 continue;
										 }
										 std::erase_if(current, [id](const std::pair<int, int>& e) { return e.first == id; });
										 current.push_back({id, level});
										 Enchantments::set(data, stack, current);
										 target->inventory().set(target->handSlot(0), stack); // Its client is told
										 done++;
									 }
									 if (done > 0) message(server, player, "Applied enchantment " + name.substr(name.find(':') + 1) + " to " + std::to_string(done) + " item(s)");
								 }});
	}
} // namespace Commands
