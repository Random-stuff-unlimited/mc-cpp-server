#ifndef COMBAT_HPP
#define COMBAT_HPP

#include <string>

class LivingEntity;
class Level;
class Player;
class Server;
struct PlayerSpawn;
struct Vec3;

// Player health, damage, death and respawn, following vanilla's rules and packets.
namespace Combat {
	struct DamageSource {
		std::string type;			  // minecraft:damage_type entry, e.g. "minecraft:player_attack", "minecraft:fall"
		Player*		attacker = nullptr; // Entity responsible, if any
	};

	// INTERACT (attack): damage from the held item and attack strength, critical hits, knockback
	void attack(Server& server, Player& attacker, Player& target);
	// Player.attack on a mob: the same damage and cooldown, critical hits, sprint knockback
	void attack(Server& server, Player& attacker, LivingEntity& target);
	// Applies damage after armor. Returns false if it had no effect (invulnerable, creative, already dead...)
	bool damage(Server& server, Player& victim, float amount, const DamageSource& source);

	// After a movement packet: fall damage when landing, void damage below the world
	void onMove(Server& server, Player& player, double previousY);
	// Where the player respawns: its bed or respawn anchor (home) if it is still there, else the world spawn
	// (fallback). `invalid` tells that the home was missing or obstructed (the "block.minecraft.spawn.not_valid" message)
	Vec3 respawnPosition(Level& level, const Vec3& fallback, const PlayerSpawn& home, bool& invalid);
	// CLIENT_COMMAND (perform respawn) after dying
	void respawn(Server& server, Player& player);
	// Every tick: end of combat (natural regeneration is FoodData's, see Survival)
	void tick(Server& server, Player& player);

	// Full health bar etc. to the player (SET_HEALTH)
	void sendHealth(Server& server, Player& player);
} // namespace Combat

#endif
