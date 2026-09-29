#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "world/World.hpp"

namespace Commands {
	void registerWeather() {
		commandList().push_back({"weather", "weather <clear|rain|thunder>", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (args.size() != 1) {
									  usage(server, player, "weather <clear|rain|thunder>");
									  return;
								  }
								  const std::string& mode = args[0];
								  if (mode == "clear") {
									  server.getWorld().setWeather(false, false);
								  } else if (mode == "rain") {
									  server.getWorld().setWeather(true, false);
								  } else if (mode == "thunder") {
									  server.getWorld().setWeather(true, true);
								  } else {
									  message(server, player, "Unknown weather: " + mode);
									  return;
								  }
								  message(server, player, "Set the weather to " + mode);
							  }});
	}
} // namespace Commands