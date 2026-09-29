#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"

namespace Commands {
	void registerKill() {
		commandList().push_back({"kill", "kill [<player>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (args.size() > 1) {
									  usage(server, player, "kill [<player>]");
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
								  // generic_kill bypasses invulnerability: even creative and spectator players die
								  if (Combat::damage(server, *target, 1.0e9F, {"minecraft:generic_kill", nullptr, nullptr, std::nullopt})) {
									  message(server, player, "Killed " + target->getPlayerName());
								  } else {
									  message(server, player, "Could not kill " + target->getPlayerName());
								  }
							  }});
	}
} // namespace Commands