#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/World.hpp"

namespace Commands {
	void registerSetWorldSpawn() {
		commandList().push_back({// Moves the world spawn to the player, or to the coordinates given
								  "setworldspawn", "setworldspawn [<x> <y> <z>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
									  double x = player.getX(), y = player.getY(), z = player.getZ();
									  if (args.size() == 3) {
										  if (!parseCoord(player.getX(), args[0], x) || !parseCoord(player.getY(), args[1], y) ||
											  !parseCoord(player.getZ(), args[2], z)) {
											  usage(server, player, "setworldspawn [<x> <y> <z>]");
											  return;
										  }
									  } else if (!args.empty()) {
										  usage(server, player, "setworldspawn [<x> <y> <z>]");
										  return;
									  }
									  server.getWorld().setSpawn(x, y, z);
									  message(server, player, "World spawn set to " + coords(x, y, z));
								  }});
	}
} // namespace Commands