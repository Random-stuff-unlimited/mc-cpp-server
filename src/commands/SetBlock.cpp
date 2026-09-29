#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "player.hpp"
#include "world/BlockPos.hpp"
#include "world/Level.hpp"

#include <cmath>

namespace Commands {
	void registerSetBlock() {
		commandList().push_back({"setblock", "setblock <x> <y> <z> <block> [destroy|keep|replace]",
								 [](Server& server, Player& player, const std::vector<std::string>& args) {
									 const char* usageText = "setblock <x> <y> <z> <block> [destroy|keep|replace]";
									 if (args.size() < 4 || args.size() > 5) {
										 usage(server, player, usageText);
										 return;
									 }
									 double x, y, z;
									 if (!parseCoord(player.getX(), args[0], x) || !parseCoord(player.getY(), args[1], y) ||
										 !parseCoord(player.getZ(), args[2], z)) {
										 usage(server, player, usageText);
										 return;
									 }
									 std::string blockName = args[3].find(':') == std::string::npos ? "minecraft:" + args[3] : args[3];
									 std::string mode	 = "replace";
									 if (args.size() == 5) {
										 mode = args[4];
										 if (mode != "destroy" && mode != "keep" && mode != "replace") {
											 message(server, player, "Unknown setblock mode: " + mode);
											 return;
										 }
									 }

									 Level* level = player.level();
									 if (!level) return;
									 int state = level->gameData().getBlockStateFromName(blockName);
									 if (state < 0) {
										 message(server, player, "Unknown block: " + blockName);
										 return;
									 }

									 BlockPos pos{static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(z))};
									 if (mode == "keep" && !level->blocks().isAir(level->getBlockState(pos))) {
										 message(server, player, "Block not placed (the position is already occupied)");
										 return;
									 }
									 if (level->setBlock(pos, state, Level::UPDATE_ALL)) {
										 message(server, player, "Block placed");
									 } else {
										 message(server, player, "Block not placed");
									 }
								 }});
	}
} // namespace Commands