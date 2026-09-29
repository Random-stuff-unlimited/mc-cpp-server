#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"

namespace Commands {
	void registerList() {
		commandList().push_back({"list", "list", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (!args.empty()) {
									  usage(server, player, "list");
									  return;
								  }
								  const auto& players = server.getGamePlayers();
								  std::string names;
								  for (const auto& other : players) {
									  if (!names.empty()) names += ", ";
									  names += other->getPlayerName();
								  }
								  message(server, player, "There are " + std::to_string(players.size()) + "/" +
												std::to_string(server.getConfig().getServerSize()) + " players online: " + names);
							  }});
	}
} // namespace Commands