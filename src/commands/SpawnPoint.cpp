#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"

#include <cmath>

namespace Commands {
	void registerSpawnPoint() {
		commandList().push_back({// Sets the player's own respawn point (their bed), at their position or the coordinates given
								  "spawnpoint", "spawnpoint [<x> <y> <z>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
									  double x = player.getX(), y = player.getY(), z = player.getZ();
									  if (args.size() == 3) {
										  if (!parseCoord(player.getX(), args[0], x) || !parseCoord(player.getY(), args[1], y) ||
											  !parseCoord(player.getZ(), args[2], z)) {
											  usage(server, player, "spawnpoint [<x> <y> <z>]");
											  return;
										  }
									  } else if (!args.empty()) {
										  usage(server, player, "spawnpoint [<x> <y> <z>]");
										  return;
									  }
									  player.spawn() = {true, static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)),
														static_cast<int>(std::floor(z)), server.levelOf(player).dimensionName(), true};
									  message(server, player, "Respawn point set to " + coordinates(player));
								  }});
	}
} // namespace Commands