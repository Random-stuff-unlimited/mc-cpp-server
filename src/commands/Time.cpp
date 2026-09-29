#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "world/World.hpp"

#include <cstdlib>

namespace Commands {
	namespace {
		// The named times of day (vanilla's TimeCommand)
		bool namedTime(const std::string& arg, int64_t& out) {
			if (arg == "day") {
				out = 1000;
				return true;
			}
			if (arg == "noon") {
				out = 6000;
				return true;
			}
			if (arg == "night") {
				out = 13000;
				return true;
			}
			if (arg == "midnight") {
				out = 18000;
				return true;
			}
			return false;
		}
	} // namespace

	void registerTime() {
		commandList().push_back({"time", "time set|add <value> | time query daytime|gametime",
								 [](Server& server, Player& player, const std::vector<std::string>& args) {
									 const char* usageText = "time set|add <value> | time query daytime|gametime";
									 if (args.size() != 2) {
										 usage(server, player, usageText);
										 return;
									 }
									 World& world = server.getWorld();

									 if (args[0] == "query") {
										 if (args[1] != "daytime" && args[1] != "gametime") {
											 usage(server, player, usageText);
											 return;
										 }
										 int64_t value = args[1] == "daytime" ? world.getDayTime() % 24000 : world.getGameTime();
										 message(server, player, "The time is " + std::to_string(value));
										 return;
									 }

									 int64_t value;
									 if (!namedTime(args[1], value)) {
										 char* end = nullptr;
										 value	 = std::strtoll(args[1].c_str(), &end, 10);
										 if (end == args[1].c_str() || *end != '\0') {
											 usage(server, player, usageText);
											 return;
										 }
									 }

									 if (args[0] == "set") {
										 world.setDayTime(value);
										 message(server, player, "Set the time to " + std::to_string(value));
									 } else if (args[0] == "add") {
										 world.setDayTime(world.getDayTime() + value);
										 message(server, player, "Added " + std::to_string(value) + " to the time");
									 } else {
										 usage(server, player, usageText);
									 }
								 }});
	}
} // namespace Commands