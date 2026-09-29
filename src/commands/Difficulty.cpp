#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"

namespace Commands {
	void registerDifficulty() {
		commandList().push_back({"difficulty", "difficulty <peaceful|easy|normal|hard>",
								 [](Server& server, Player& player, const std::vector<std::string>& args) {
									 if (args.size() != 1) {
										 usage(server, player, "difficulty <peaceful|easy|normal|hard>");
										 return;
									 }
									 const std::string& name = args[0];
									 if (name != "peaceful" && name != "easy" && name != "normal" && name != "hard") {
										 message(server, player, "Unknown difficulty: " + name);
										 return;
									 }
									 server.getConfig().setDifficulty(name);
									 for (const auto& other : server.getGamePlayers()) {
										 Packet packet(other->shared_from_this());
										 changeDifficultyPacket(packet, server);
									 }
									 message(server, player, "Set the difficulty to " + name);
								 }});
	}
} // namespace Commands