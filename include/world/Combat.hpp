#ifndef COMBAT_HPP
#define COMBAT_HPP

#include "world/BlockPos.hpp"
#include "world/entity/Actor.hpp"
#include "world/entity/Geometry.hpp"

#include <optional>
#include <string>

class GameData;
class LivingEntity;
class Level;
class Player;
class Server;
struct PlayerSpawn;

// Player health, damage, death and respawn, following vanilla's rules and packets.
namespace Combat {
	// Vanilla's DamageSource: the type, the entity responsible (getEntity) and the one that dealt it (getDirectEntity:
	// an arrow, a primed TNT; the responsible one itself when null), and where it came from when no entity did
	struct DamageSource {
		std::string			type;			   // minecraft:damage_type entry, e.g. "minecraft:player_attack", "minecraft:fall"
		Actor*				causing = nullptr; // getEntity: the player or mob responsible, if any
		Actor*				direct	= nullptr; // getDirectEntity when it isn't the responsible one
		std::optional<Vec3> position;		   // sourcePositionRaw (bad respawn point explosions)

		Actor*				directEntity() const { return direct ? direct : causing; }
		// The player responsible, if a player is
		Player*				attackerPlayer() const { return causing ? causing->asPlayer() : nullptr; }
		// DamageSource.getSourcePosition: the given position, else the direct entity's
		std::optional<Vec3> sourcePosition() const {
			if (position) return position;
			if (Actor* d = directEntity()) return d->position();
			return std::nullopt;
		}
		// DamageSource.isDirect
		bool isDirect() const { return direct == nullptr || direct == causing; }
	};

	// INTERACT (attack): damage from the held item and attack strength, critical hits, knockback
	void attack(Server& server, Player& attacker, Player& target);
	// Player.attack on a mob: the same damage and cooldown, critical hits, sprint knockback
	void attack(Server& server, Player& attacker, LivingEntity& target);
	// Applies damage after armor. Returns false if it had no effect (invulnerable, creative, already dead...)
	bool damage(Server& server, Player& victim, float amount, const DamageSource& source);

	// After a movement packet: fall damage when landing, void damage below the world
	void onMove(Server& server, Player& player, double previousY);
	// Where the player respawns at its bed or respawn anchor (vanilla's ServerPlayer.findRespawnAndUseSpawnBlock),
	// home being in `level`: next to the bed, around the anchor (using a charge if useCharge), or where /spawnpoint
	// set it. nullopt if the block is gone, obstructed, or doesn't work in this dimension
	struct RespawnPos {
		Vec3  position;
		float yaw, pitch;
	};
	std::optional<RespawnPos> findRespawnAndUseSpawnBlock(Level& level, const PlayerSpawn& home, bool useCharge);
	// BedBlock.findStandUpPosition: a safe spot around the bed at pos (facing: the bed's), for a player looking at yaw
	std::optional<Vec3> findBedStandUpPosition(Level& level, const BlockPos& pos, Direction facing, float yaw);
	// CLIENT_COMMAND (perform respawn): after dying, or after the end credits (keepAllData: health, food... kept, and
	// the respawn anchor keeps its charge)
	void respawn(Server& server, Player& player, bool keepAllData = false);
	// Every tick: end of combat (natural regeneration is FoodData's, see Survival)
	void tick(Server& server, Player& player);

	// Full health bar etc. to the player (SET_HEALTH)
	void sendHealth(Server& server, Player& player);
	// The name of an actor in messages: a player's name, else its type in English ("minecraft:cave_spider" -> "Cave
	// Spider")
	std::string displayName(const GameData& gameData, const Actor& actor);
} // namespace Combat

#endif
