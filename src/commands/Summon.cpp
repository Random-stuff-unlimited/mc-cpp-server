#include "Commands.hpp"

#include "commands/CommandSupport.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/EntityFactory.hpp"
#include "world/entity/Mob.hpp"
#include "world/entity/MobRegistry.hpp"


// SummonCommand: /summon <entity> [<x> <y> <z>]. Mobs are made like vanilla's (finalizeSpawn, EntitySpawnReason.COMMAND);
// the other entities the server simulates (TNT, end crystals...) are put there as they are
namespace Commands {
	void registerSummon() {
		commandList().push_back({"summon", "summon <entity> [<x> <y> <z>]", [](Server& server, Player& player, const std::vector<std::string>& args) {
									 if (args.size() != 1 && args.size() != 4) {
										 usage(server, player, "summon <entity> [<x> <y> <z>]");
										 return;
									 }
									 std::string type   = args[0].find(':') == std::string::npos ? "minecraft:" + args[0] : args[0];
									 Level&			 level = server.levelOf(player);
									 const GameData& data  = level.gameData();
									 int		 typeId = data.getStaticId("minecraft:entity_type", type);
									 const GameData::EntityTypeInfo* info = typeId >= 0 ? data.getEntityType(typeId) : nullptr;
									 if (!info || !info->summonable) {
										 message(server, player, "Unknown entity: " + type);
										 return;
									 }
									 double x = player.getX(), y = player.getY(), z = player.getZ();
									 if (args.size() == 4 && (!parseCoord(player.getX(), args[1], x) || !parseCoord(player.getY(), args[2], y) ||
															  !parseCoord(player.getZ(), args[3], z))) {
										 usage(server, player, "summon <entity> [<x> <y> <z>]");
										 return;
									 }
									 Vec3	 at{x, y, z};
									 BlockPos block{Mth::floor(x), Mth::floor(y), Mth::floor(z)};
									 if (info->mob) {
										 // In peaceful, monsters aren't summoned (vanilla: "Unable to summon entity")
										 if (!info->allowedInPeaceful && level.difficulty() == 0) {
											 message(server, player, "Unable to summon entity: no monsters in peaceful");
											 return;
										 }
										 Mob* mob = level.mobs().spawn(level, typeId, block, MobRegistry::SpawnReason::Command);
										 if (!mob) {
											 message(server, player, "Unable to summon entity");
											 return;
										 }
										 mob->setPos(at);
										 mob->snapTo(at, mob->yRot(), mob->xRot());
									 } else {
										 std::unique_ptr<Entity> entity = EntityFactory::create(level, typeId);
										 if (!entity) {
											 message(server, player, "Unable to summon entity: " + type + " isn't simulated here");
											 return;
										 }
										 entity->setPos(at);
										 level.entities().add(std::move(entity));
									 }
									 message(server, player, "Summoned new " + type.substr(type.find(':') + 1));
								 }});
	}
} // namespace Commands
