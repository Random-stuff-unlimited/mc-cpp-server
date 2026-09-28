#ifndef SURVIVAL_HPP
#define SURVIVAL_HPP

class Buffer;
class Level;
class Player;
class Server;

// Hunger, natural regeneration, breathing and the player's own state vanilla keeps in LivingEntity / Player /
// ServerPlayer: FoodData.tick, LivingEntity.baseTick (air, drowning), the water state of Entity.baseTick, the pose,
// the exhaustion of movements, jumps, attacks, damage and mining, and SET_HEALTH / SET_ENTITY_DATA when they change.
// Game thread only.
namespace Survival {
	enum class Difficulty { Peaceful = 0, Easy = 1, Normal = 2, Hard = 3 };

	// Entity data entries sent when they change (SurvivalState.dirtyData bits)
	constexpr int DATA_AIR_SUPPLY	= 1; // Entity.DATA_AIR_SUPPLY_ID
	constexpr int DATA_LIVING_FLAGS = 2; // LivingEntity.DATA_LIVING_ENTITY_FLAGS
	constexpr int MAX_AIR_SUPPLY	= 300;

	// The world's difficulty (config "difficulty": peaceful, easy, normal, hard)
	Difficulty difficulty(Server& server);

	// Player.causeFoodExhaustion: nothing for invulnerable players (creative, spectator)
	void causeFoodExhaustion(Player& player, float exhaustion);
	// LivingEntity.heal
	void heal(Player& player, float amount);
	// Player.isHurt
	bool isHurt(const Player& player);
	// Entity.getEyeHeight of the current pose: 1.62 standing, 1.27 crouching, 0.4 swimming
	double eyeHeight(const Player& player);

	// ServerPlayer.jumpFromGround: the exhaustion of a jump
	void jumpFromGround(Player& player);
	// ServerPlayer.checkMovementStatistics: exhaustion of the distance moved (sprinting, swimming, in water)
	void checkMovementStatistics(Level& level, Player& player, double dx, double dy, double dz);
	// ServerGamePacketListenerImpl.handleMovePlayer's part: jump (on the ground, now in the air and going up), then the
	// movement. Called with the position already set, before onGround is updated
	void onMove(Level& level, Player& player, double dx, double dy, double dz, bool packetOnGround);

	// Every tick (ServerPlayer.doTick): water state, air and drowning, the used item, peaceful regeneration, pose,
	// FoodData.tick, then SET_HEALTH if health, food or "saturation is zero" changed
	void tick(Level& level, Player& player);
	// FoodData.tick alone
	void tickFood(Level& level, Player& player);
	// The entity data that changed this tick, to the player and the ones that see it (ServerEntity.sendDirtyEntityData)
	void sendDirtyData(Server& server, Player& player);
	// The entity data that isn't at its default value, for a new viewer (ADD_ENTITY's getNonDefaultValues). Writes
	// nothing and returns false when everything is at its default
	bool writeNonDefaultData(Buffer& data, const Player& player);
	// SET_HEALTH now, remembered like ServerPlayer.lastSentHealth / lastSentFood
	void sendHealth(Server& server, Player& player);
	// After a respawn: everything again (ServerPlayer.restoreFrom / reset: lastSentHealth = -1...)
	void reset(Player& player);
} // namespace Survival

#endif
