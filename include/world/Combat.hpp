#ifndef COMBAT_HPP
#define COMBAT_HPP

#include <string>

class Player;
class Server;

// Player health, damage, death and respawn, following vanilla's rules and packets.
namespace Combat {
	struct DamageSource {
		std::string type;			  // minecraft:damage_type entry, e.g. "minecraft:player_attack", "minecraft:fall"
		std::string deathMessage;	  // Translation key, e.g. "death.attack.player": %1$s = victim, %2$s = attacker
		Player*		attacker = nullptr; // Entity responsible, if any
	};

	// INTERACT (attack): damage from the held item and attack strength, critical hits, knockback
	void attack(Server& server, Player& attacker, Player& target);
	// Applies damage after armor. Returns false if it had no effect (invulnerable, creative, already dead...)
	bool damage(Server& server, Player& victim, float amount, const DamageSource& source);

	// After a movement packet: fall damage when landing, void damage below the world
	void onMove(Server& server, Player& player, double previousY);
	// CLIENT_COMMAND (perform respawn) after dying
	void respawn(Server& server, Player& player);
	// Once per second: natural regeneration, end of combat
	void tick(Server& server, Player& player);

	// Full health bar etc. to the player (SET_HEALTH)
	void sendHealth(Server& server, Player& player);
} // namespace Combat

#endif
