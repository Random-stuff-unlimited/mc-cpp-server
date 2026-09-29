#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/item/ItemStack.hpp"

#include <cstdlib>

namespace Commands {
	void registerGive() {
		commandList().push_back({"give", "give <player> <item> [count]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (args.size() < 2 || args.size() > 3) {
									  usage(server, player, "give <player> <item> [count]");
									  return;
								  }
								  auto found = server.findPlayersByName(args[0]);
								  if (found.empty()) {
									  message(server, player, "Player not found: " + args[0]);
									  return;
								  }
								  Player* target = found[0].get();

								  std::string itemName = args[1].find(':') == std::string::npos ? "minecraft:" + args[1] : args[1];
								  int		 itemId	 = server.getGameData().getStaticId("minecraft:item", itemName);
								  if (itemId < 0) {
									  message(server, player, "Unknown item: " + itemName);
									  return;
								  }

								  int count = 1;
								  if (args.size() == 3) {
									  char* end = nullptr;
									  count	 = static_cast<int>(std::strtol(args[2].c_str(), &end, 10));
									  if (end == args[2].c_str() || *end != '\0' || count < 1) {
										  usage(server, player, "give <player> <item> [count]");
										  return;
									  }
								  }

								  ItemStack stack(itemId, count);
								  target->inventory().add(stack, target->getSelectedSlot(), target->isCreative(), server.getGameData());
								  if (!stack.isEmpty()) server.levelOf(*target).dropFromPlayer(*target, stack, false);
								  message(server, player, "Gave " + std::to_string(count) + " " + itemName + " to " + target->getPlayerName());
							  }});
	}
} // namespace Commands