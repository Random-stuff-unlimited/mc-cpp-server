#include "Commands.hpp"

#include "commands/CommandSupport.hpp"

namespace Commands {
	void registerHelp() {
		commandList().push_back({"help", "help [<command>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
								  if (!args.empty()) {
									  for (const Command& command : commandList()) {
										  if (command.name == args[0]) {
											  message(server, player, "/" + command.usage);
											  return;
										  }
									  }
									  message(server, player, "Unknown command: " + args[0]);
									  return;
								  }
								  std::string list;
								  for (const Command& command : commandList()) {
									  if (!list.empty()) list += ", ";
									  list += "/" + command.name;
								  }
								  message(server, player, "Commands: " + list);
							  }});
	}
} // namespace Commands