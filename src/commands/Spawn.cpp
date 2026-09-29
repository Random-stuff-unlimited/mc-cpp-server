#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/World.hpp"

namespace Commands {
	void registerSpawn() {
		commandList().push_back({// Teleports to the world spawn
								  "spawn", "spawn", [](Server& server, Player& player, const std::vector<std::string>& args) {
									  if (!args.empty()) {
										  usage(server, player, "spawn");
										  return;
									  }
									  const World::Spawn& spawn = server.getWorld().getSpawn();
									  if (player.level() != &server.getLevel()) {
										  server.changeDimension(player, server.getLevel(), spawn.x, spawn.y, spawn.z, player.getYaw(), player.getPitch());
									  } else {
										  teleport(server, player, spawn.x, spawn.y, spawn.z);
									  }
									  message(server, player, "Teleported to the world spawn");
								  }});
	}
} // namespace Commands