#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/item/ItemStack.hpp"
#include "world/item/PlayerInventory.hpp"

namespace Commands {
	void registerClear() {
		commandList().push_back({"clear", "clear [<player>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (args.size() > 1) {
									  usage(server, player, "clear [<player>]");
									  return;
								  }
								  Player* target = &player;
								  if (!args.empty()) {
									  auto found = server.findPlayersByName(args[0]);
									  if (found.empty()) {
										  message(server, player, "Player not found: " + args[0]);
										  return;
									  }
									  target = found[0].get();
								  }
								  int cleared = 0;
								  for (int slot = 0; slot < PlayerInventory::SIZE; ++slot) {
									  if (!target->inventory().get(slot).isEmpty()) {
										  target->inventory().set(slot, ItemStack());
										  ++cleared;
									  }
								  }
								  message(server, player,
										  "Cleared " + std::to_string(cleared) + " item(s)" + (target == &player ? "" : " from " + target->getPlayerName()));
							  }});
	}
} // namespace Commands