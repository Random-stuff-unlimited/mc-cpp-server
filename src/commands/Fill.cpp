#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "player.hpp"
#include "world/BlockPos.hpp"
#include "world/Level.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace Commands {
	void registerFill() {
		commandList().push_back({
			"fill",
			"fill <x1> <y1> <z1> <x2> <y2> <z2> <block> [destroy|hollow|keep|outline|replace|strict] [filter]",
			[](Server& server, Player& player, const std::vector<std::string>& args) {
				const char* usageText = "Usage: /fill <x1> <y1> <z1> <x2> <y2> <z2> <block> [destroy|hollow|keep|outline|replace] [filter]";

				// 1. Vérification du nombre d'arguments (minimum 7 pour les coords et le bloc cible)
				if (args.size() < 7 || args.size() > 9) {
					usage(server, player, usageText);
					return;
				}

				// 2. Parsing des coordonnées de départ (<from>)
				double x1, y1, z1;
				if (!parseCoord(player.getX(), args[0], x1) ||
					!parseCoord(player.getY(), args[1], y1) ||
					!parseCoord(player.getZ(), args[2], z1)) {
					usage(server, player, usageText);
					return;
				}

				// 3. Parsing des coordonnées d'arrivée (<to>)
				double x2, y2, z2;
				if (!parseCoord(player.getX(), args[3], x2) ||
					!parseCoord(player.getY(), args[4], y2) ||
					!parseCoord(player.getZ(), args[5], z2)) {
					usage(server, player, usageText);
					return;
				}

				// 4. Parsing du bloc à placer
				// On s'assure d'avoir le namespace "minecraft:" s'il n'est pas précisé
				std::string blockName = args[6].find(':') == std::string::npos ? "minecraft:" + args[6] : args[6];

				// 5. Parsing du mode d'action (optionnel)
				std::string mode = "replace"; // Mode par défaut dans Minecraft Vanilla
				if (args.size() >= 8) {
					mode = args[7];

					// Vérification de la validité du mode
					if (mode != "destroy" && mode != "hollow" && mode != "keep" &&
						mode != "outline" && mode != "replace" && mode != "strict") {
						message(server, player, "Unknown fill mode: " + mode);
						return;
					}
				}

				// 6. Parsing du filtre de remplacement (uniquement utilisable avec le mode "replace")
				std::string replaceFilter = "";
				if (args.size() == 9) {
					if (mode != "replace") {
						message(server, player, "A block filter can only be specified when using 'replace' mode");
						return;
					}
					replaceFilter = args[8].find(':') == std::string::npos ? "minecraft:" + args[8] : args[8];
				}

				// --- FIN DU PARSING ---

				Level* level = player.level();
				if (!level) return;

				int blockState = level->gameData().getBlockStateFromName(blockName);
				if (blockState < 0) {
					message(server, player, "Unknown block: " + blockName);
					return;
				}
				int filterState = -1;
				if (!replaceFilter.empty()) {
					filterState = level->gameData().getBlockStateFromName(replaceFilter);
					if (filterState < 0) {
						message(server, player, "Unknown block: " + replaceFilter);
						return;
					}
				}

				// The region, its coordinates rounded down like vanilla (new BlockPos floors doubles)
				int minX = static_cast<int>(std::floor(std::min(x1, x2))), maxX = static_cast<int>(std::floor(std::max(x1, x2)));
				int minY = static_cast<int>(std::floor(std::min(y1, y2))), maxY = static_cast<int>(std::floor(std::max(y1, y2)));
				int minZ = static_cast<int>(std::floor(std::min(z1, z2))), maxZ = static_cast<int>(std::floor(std::max(z1, z2)));
				// Clamped to the dimension's build height (out-of-bounds blocks aren't placed, as in vanilla)
				minY = std::max(minY, level->minY());
				maxY = std::min(maxY, level->maxY() - 1);

				int64_t volume = static_cast<int64_t>(maxX - minX + 1) * std::max(0, maxY - minY + 1) * (maxZ - minZ + 1);
				if (volume > 32768) {
					message(server, player, "The fill region is too big (max 32768 blocks)");
					return;
				}

				int airState = level->gameData().getDefaultBlockState("minecraft:air");
				bool hollow	 = mode == "hollow";
				bool keep	 = mode == "keep";
				bool outline = mode == "outline";

				int changed = 0;
				for (int y = minY; y <= maxY; ++y) {
					for (int z = minZ; z <= maxZ; ++z) {
						for (int x = minX; x <= maxX; ++x) {
							// Hollow and outline fill the border only: the block there, nothing inside
							bool edge = x == minX || x == maxX || y == minY || y == maxY || z == minZ || z == maxZ;
							if (!edge && outline) continue;
							int place = !edge && hollow ? airState : blockState;

							int old = level->getBlockState({x, y, z});
							if (keep && !level->blocks().isAir(old)) continue;
							if (filterState >= 0 && old != filterState) continue;

							if (level->setBlock({x, y, z}, place, Level::UPDATE_ALL)) ++changed;
						}
					}
				}

				message(server, player, "Filled " + std::to_string(changed) + " blocks");
			}
		});
	}
} // namespace Commands