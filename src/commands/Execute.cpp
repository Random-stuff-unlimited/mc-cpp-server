#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"

namespace Commands {
	void registerExecute() {
		commandList().push_back({// The vanilla form that changes dimension: execute in <dimension> run tp [<x> <y> <z>]
								  "execute", "execute in <dimension> run tp [<x> <y> <z>]",
								  [](Server& server, Player& player, const std::vector<std::string>& args) {
									  const char* usageText = "execute in <dimension> run tp [<x> <y> <z>]";
									  if ((args.size() != 4 && args.size() != 7) || args[0] != "in" || args[2] != "run" || args[3] != "tp") {
										  usage(server, player, usageText);
										  return;
									  }
									  std::string name = args[1].find(':') == std::string::npos ? "minecraft:" + args[1] : args[1];
									  Level*	  destination = server.getLevel(name);
									  if (!destination) {
										  message(server, player, "Unknown dimension: " + args[1]);
										  return;
									  }
									  // ~ is relative to the player, its coordinates kept as they are (like vanilla's execute in)
									  double x = player.getX(), y = player.getY(), z = player.getZ();
									  if (args.size() == 7 && (!parseCoord(player.getX(), args[4], x) || !parseCoord(player.getY(), args[5], y) ||
															   !parseCoord(player.getZ(), args[6], z))) {
										  usage(server, player, usageText);
										  return;
									  }
									  if (destination == player.level()) {
										  teleport(server, player, x, y, z);
									  } else {
										  server.changeDimension(player, *destination, x, y, z, player.getYaw(), player.getPitch());
									  }
									  message(server, player, "Teleported to " + coords(x, y, z) + " in " + name);
								  }});
	}
} // namespace Commands