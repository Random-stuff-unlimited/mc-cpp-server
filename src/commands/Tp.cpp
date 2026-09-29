#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"

namespace Commands {
	void registerTp() {
		commandList().push_back({// Teleports the player (or another) to absolute or relative (with ~) coordinates
								  "tp", "tp [<player>] <x> <y> <z>", [](Server& server, Player& player, const std::vector<std::string>& args) {
									  Player* target = &player;
									  size_t	start = 0;
									  if (args.size() == 4) {
										  auto found = server.findPlayersByName(args[0]);
										  if (found.empty()) {
											  message(server, player, "Player not found: " + args[0]);
											  return;
										  }
										  target = found[0].get();
										  start	 = 1;
									  }
									  if (args.size() != start + 3) {
										  usage(server, player, "tp [<player>] <x> <y> <z>");
										  return;
									  }
									  double x, y, z;
									  if (!parseCoord(target->getX(), args[start], x) || !parseCoord(target->getY(), args[start + 1], y) ||
										  !parseCoord(target->getZ(), args[start + 2], z)) {
										  usage(server, player, "tp [<player>] <x> <y> <z>");
										  return;
									  }
									  teleport(server, *target, x, y, z);
									  message(server, player,
											  (target == &player ? "Teleported to " : "Teleported " + target->getPlayerName() + " to ") + coordinates(*target));
								  }});
	}
} // namespace Commands