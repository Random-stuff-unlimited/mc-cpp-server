#include "world/Combat.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/TextComponent.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/packetRouter.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/ChunkStreamer.hpp"
#include "world/PlayerTracker.hpp"
#include "world/World.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
	constexpr float	  MAX_HEALTH			= 20;
	constexpr int64_t INVULNERABILITY_TICKS = 10;
	constexpr int64_t REGENERATION_TICKS	= 80;  // When food >= 18
	constexpr int64_t COMBAT_END_TICKS		= 300; // Without damage
	constexpr double  BASE_KNOCKBACK		= 0.4;
	constexpr double  SPRINT_KNOCKBACK		= 0.5;
	constexpr double  EYE_HEIGHT			= 1.62;
	constexpr double  PLAYER_HALF_WIDTH		= 0.3;
	constexpr double  PLAYER_HEIGHT			= 1.8;
	constexpr int	  ENTITY_EVENT_DEATH	= 3;
	constexpr int	  ANIMATE_SWING_MAIN	= 0;
	constexpr int	  ANIMATE_CRITICAL_HIT	= 4;
	constexpr int	  GAME_EVENT_WAIT_CHUNKS = 13;
	constexpr int	  HEALTH_DATA_INDEX		= 9; // LivingEntity.DATA_HEALTH_ID
	constexpr int	  FLOAT_SERIALIZER		= 3; // EntityDataSerializers.FLOAT
	constexpr int64_t KILL_CREDIT_TICKS		= 100; // Vanilla lastHurtByPlayer memory

	// Player inventory window slots of the armor
	constexpr int ARMOR_SLOTS[4] = {5, 6, 7, 8};
	constexpr GameData::EquipmentSlot ARMOR_SLOT_TYPES[4] = {GameData::EquipmentSlot::Head, GameData::EquipmentSlot::Chest,
															 GameData::EquipmentSlot::Legs, GameData::EquipmentSlot::Feet};

	int64_t currentTick(Server& server) { return server.getTickLoop().getTickCount(); }

	struct Armor {
		float armor = 0, toughness = 0, knockbackResistance = 0;
	};

	Armor wornArmor(const GameData& gameData, const Player& player) {
		Armor total;
		for (int i = 0; i < 4; i++) {
			const GameData::ItemProperties* item = gameData.getItemProperties(player.getInventoryItem(ARMOR_SLOTS[i]));
			if (!item || item->equipmentSlot != ARMOR_SLOT_TYPES[i]) continue;
			total.armor += item->armor;
			total.toughness += item->armorToughness;
			total.knockbackResistance += item->knockbackResistance;
		}
		total.knockbackResistance = std::min(total.knockbackResistance, 1.0f);
		return total;
	}

	// Vanilla CombatRules.getDamageAfterAbsorb
	float afterArmor(float damage, const Armor& armor) {
		float toughness = 2.0f + armor.toughness / 4.0f;
		float effective = std::clamp(armor.armor - damage / toughness, armor.armor * 0.2f, 20.0f);
		return damage * (1.0f - effective / 25.0f);
	}

	// Distance from the attacker's eyes to the closest point of the target's hitbox
	double reachDistance(const Player& attacker, const Player& target) {
		double eyeX = attacker.getX(), eyeY = attacker.getY() + EYE_HEIGHT, eyeZ = attacker.getZ();
		double x = std::clamp(eyeX, target.getX() - PLAYER_HALF_WIDTH, target.getX() + PLAYER_HALF_WIDTH);
		double y = std::clamp(eyeY, target.getY(), target.getY() + PLAYER_HEIGHT);
		double z = std::clamp(eyeZ, target.getZ() - PLAYER_HALF_WIDTH, target.getZ() + PLAYER_HALF_WIDTH);
		return std::sqrt((x - eyeX) * (x - eyeX) + (y - eyeY) * (y - eyeY) + (z - eyeZ) * (z - eyeZ));
	}

	void sendHealthValues(Server& server, Player& player, float health, int food, float saturation) {
		Buffer buf;
		buf.writeFloat(health);
		buf.writeVarInt(food);
		buf.writeFloat(saturation);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SET_HEALTH, buf, server);
	}

	void die(Server& server, Player& victim, const Combat::DamageSource& source) {
		PlayerTracker& tracker = server.getPlayerTracker();
		int			   id	   = victim.getPlayerID();

		// Others see the body fall: health 0 in the entity data, then the death event
		Buffer data;
		data.writeVarInt(id);
		data.writeUByte(HEALTH_DATA_INDEX);
		data.writeVarInt(FLOAT_SERIALIZER);
		data.writeFloat(0);
		data.writeUByte(0xFF); // End of the entity data
		tracker.broadcast(&victim, PacketId::Play::Clientbound::SET_ENTITY_DATA, data, false);
		Buffer event;
		event.writeInt(id);
		event.writeByte(ENTITY_EVENT_DEATH);
		tracker.broadcast(&victim, PacketId::Play::Clientbound::ENTITY_EVENT, event, false);

		// Message from death-messages/, in each player's language. The killer is the player that hurt the victim
		// recently, if any
		const CombatState& state  = victim.combat();
		bool			   credit = !state.lastAttacker.empty() && currentTick(server) - state.lastAttackedAt <= KILL_CREDIT_TICKS;
		std::string		   victimName = victim.getPlayerName();
		std::string		   sourceName = source.attacker ? source.attacker->getPlayerName() : "";
		std::string		   killerName = credit ? state.lastAttacker : "";
		auto			   text		  = [&](const Player& reader) {
			  return server.getDeathMessages().format(reader.getPlayerConfig()->getLocale(), source.type, victimName, sourceName, killerName);
		};

		// Death screen with the message
		Buffer screen;
		screen.writeVarInt(id);
		TextComponent::writeText(screen, text(victim));
		Packet::send(victim.shared_from_this(), PacketId::Play::Clientbound::PLAYER_COMBAT_KILL, screen, server);

		// Then in everyone's chat: encoded once per language
		std::unordered_map<std::string, std::vector<uint8_t>> frames;
		for (const auto& player : server.getGamePlayers()) {
			std::vector<uint8_t>& frame = frames[player->getPlayerConfig()->getLocale()];
			if (frame.empty()) {
				Buffer chat;
				TextComponent::writeText(chat, text(*player));
				chat.writeBool(false); // In the chat, not above the hotbar
				frame = Packet::buildFrame(PacketId::Play::Clientbound::SYSTEM_CHAT, chat.getData(), server.getConfig().getCompressionThreshold());
			}
			Packet::sendFrame(player, frame, server);
		}
	}
} // namespace

namespace Combat {

	void sendHealth(Server& server, Player& player) {
		sendHealthValues(server, player, player.combat().health, player.combat().food, player.combat().saturation);
	}

	void attack(Server& server, Player& attacker, Player& target) {
		if (&attacker == &target || attacker.getGameMode() == GameMode::Spectator) return;
		if (attacker.combat().dead) return;
		double fallDistance = attacker.combat().fallDistance;
		// Vanilla entity interaction range: 3 blocks, 5 in creative, plus a margin for latency
		double range = (attacker.getGameMode() == GameMode::Creative ? 5.0 : 3.0) + 1.0;
		if (reachDistance(attacker, target) > range) return;

		// Damage of the held item, scaled by how charged the attack is (spamming clicks does little)
		const GameData::ItemProperties* item		= server.getGameData().getItemProperties(attacker.getItemInHand(0));
		float							baseDamage	= 1.0f + (item ? item->attackDamage : 0.0f);
		float							attackSpeed = std::max(0.1f, 4.0f + (item ? item->attackSpeed : 0.0f));
		int64_t							now			= currentTick(server);
		// Vanilla getAttackStrengthScale(0.5): ticks since the last attack against the item's cooldown
		double strength = std::clamp((now - attacker.getLastAttack() + 0.5) / (20.0 / attackSpeed), 0.0, 1.0);
		attacker.setLastAttack(now);

		float damage = baseDamage * static_cast<float>(0.2 + strength * strength * 0.8);
		bool  strong = strength > 0.9;
		bool  critical = strong && fallDistance > 0 && !attacker.isOnGround() && !attacker.isSprinting();
		if (critical) damage *= 1.5f;

		DamageSource source{"minecraft:player_attack", &attacker};
		if (!Combat::damage(server, target, damage, source)) return;

		if (strong && attacker.isSprinting()) {
			// Sprint hit: extra knockback in the attacker's look direction
			double yaw		 = attacker.getYaw() * M_PI / 180.0;
			float  resistance = wornArmor(server.getGameData(), target).knockbackResistance;
			double strength2 = SPRINT_KNOCKBACK * (1.0 - resistance);
			Buffer motion;
			motion.writeVarInt(target.getPlayerID());
			motion.writeLpVec3(-std::sin(yaw) * strength2, 0.1, std::cos(yaw) * strength2);
			server.getPlayerTracker().broadcast(&target, PacketId::Play::Clientbound::SET_ENTITY_MOTION, motion, true);
		}
		if (critical) {
			Buffer animation;
			animation.writeVarInt(target.getPlayerID());
			animation.writeUByte(ANIMATE_CRITICAL_HIT);
			server.getPlayerTracker().broadcast(&target, PacketId::Play::Clientbound::ANIMATE, animation, true);
		}
	}

	bool damage(Server& server, Player& victim, float amount, const DamageSource& source) {
		const GameData& gameData = server.getGameData();
		int				typeId	 = gameData.getSyncedId("minecraft:damage_type", source.type);
		bool bypassesInvulnerability = gameData.isInTag("minecraft:damage_type", "minecraft:bypasses_invulnerability", typeId);
		if ((victim.getGameMode() == GameMode::Creative || victim.getGameMode() == GameMode::Spectator) && !bypassesInvulnerability) return false;

		int64_t now	 = currentTick(server);
		Armor	armor = wornArmor(gameData, victim);
		float	health, saturation;
		int		food;
		bool	died = false, enteredCombat = false;
		{
			CombatState& state = victim.combat();
			if (state.dead) return false;

			// Just hit: only a stronger hit gets through, for the difference
			float applied = amount;
			if (now < state.invulnerableUntil) {
				if (amount <= state.lastDamage) return false;
				applied = amount - state.lastDamage;
			} else {
				state.invulnerableUntil = now + INVULNERABILITY_TICKS;
			}
			state.lastDamage = amount;
			if (source.attacker) {
				state.lastAttacker	 = source.attacker->getPlayerName();
				state.lastAttackedAt = now;
			}
			if (state.health >= MAX_HEALTH) state.lastRegeneration = now; // Regeneration starts counting from the first damage
			if (!gameData.isInTag("minecraft:damage_type", "minecraft:bypasses_armor", typeId)) applied = afterArmor(applied, armor);

			state.health -= applied;
			if (state.health <= 0) {
				state.health		   = 0;
				state.dead			   = true;
				state.inCombat		   = false;
				state.hasDeathLocation = true;
				state.deathX		   = static_cast<int>(std::floor(victim.getX()));
				state.deathY		   = static_cast<int>(std::floor(victim.getY()));
				state.deathZ		   = static_cast<int>(std::floor(victim.getZ()));
				died				   = true;
			} else if (source.attacker) {
				if (!state.inCombat) {
					state.inCombat	  = true;
					state.combatStart = now;
					enteredCombat	  = true;
				}
				state.lastCombat = now;
			}
			health	   = state.health;
			food	   = state.food;
			saturation = state.saturation;
		}

		sendHealthValues(server, victim, health, food, saturation);
		if (enteredCombat) {
			Buffer empty;
			Packet::send(victim.shared_from_this(), PacketId::Play::Clientbound::PLAYER_COMBAT_ENTER, empty, server);
		}

		// Everyone sees the hit (red flash, sound); ids of the source entities are sent + 1, 0 = none
		Buffer event;
		event.writeVarInt(victim.getPlayerID());
		event.writeVarInt(typeId);
		event.writeVarInt(source.attacker ? source.attacker->getPlayerID() + 1 : 0); // Cause
		event.writeVarInt(source.attacker ? source.attacker->getPlayerID() + 1 : 0); // Direct (a projectile would differ)
		event.writeBool(false);														 // No source position
		server.getPlayerTracker().broadcast(&victim, PacketId::Play::Clientbound::DAMAGE_EVENT, event, true);

		if (source.attacker) {
			// Knockback away from the attacker, and the victim's camera tilts toward the hit
			double dx = source.attacker->getX() - victim.getX();
			double dz = source.attacker->getZ() - victim.getZ();
			double length = std::sqrt(dx * dx + dz * dz);
			if (length < 1.0E-4) {
				dx	   = 0.01;
				length = 0.01;
			}
			double strength = BASE_KNOCKBACK * (1.0 - armor.knockbackResistance);
			if (strength > 0) {
				Buffer motion;
				motion.writeVarInt(victim.getPlayerID());
				motion.writeLpVec3(-dx / length * strength, victim.isOnGround() ? std::min(0.4, strength) : 0.0, -dz / length * strength);
				server.getPlayerTracker().broadcast(&victim, PacketId::Play::Clientbound::SET_ENTITY_MOTION, motion, true);
			}
			Buffer tilt;
			tilt.writeVarInt(victim.getPlayerID());
			tilt.writeFloat(static_cast<float>(std::atan2(dz, dx) * 180.0 / M_PI - victim.getYaw()));
			Packet::send(victim.shared_from_this(), PacketId::Play::Clientbound::HURT_ANIMATION, tilt, server);
		}

		if (died) die(server, victim, source);
		return true;
	}

	void onMove(Server& server, Player& player, double previousY) {
		World& world = server.getWorld();
		double y	 = player.getY();

		// Below the world: 4 damage every half second, even in creative
		if (y < world.getMinY() - 64) {
			damage(server, player, 4.0f, {"minecraft:out_of_world", nullptr});
			return;
		}
		if (player.getGameMode() == GameMode::Creative || player.getGameMode() == GameMode::Spectator) {
			player.combat().fallDistance = 0;
			return;
		}

		// Water, lava, ladders and vines stop a fall
		const GameData& gameData = server.getGameData();
		int				state	 = world.getBlock(static_cast<int>(std::floor(player.getX())), static_cast<int>(std::floor(y)),
												  static_cast<int>(std::floor(player.getZ())));
		bool			cushioned = false;
		if (state >= 0) {
			int block = gameData.getBlockOfState(state);
			cushioned = block == gameData.getStaticId("minecraft:block", "minecraft:water") ||
						block == gameData.getStaticId("minecraft:block", "minecraft:lava") ||
						gameData.isInTag("minecraft:block", "minecraft:climbable", block) || gameData.getProperty(state, "waterlogged") == "true";
		}

		double landedFrom = 0;
		{
			CombatState& combat = player.combat();
			if (combat.dead || cushioned) {
				combat.fallDistance = 0;
				return;
			}
			if (!player.isOnGround()) {
				if (y < previousY) combat.fallDistance += previousY - y;
				return;
			}
			landedFrom			= combat.fallDistance;
			combat.fallDistance = 0;
		}
		if (landedFrom > 3.0) {
			damage(server, player, static_cast<float>(std::ceil(landedFrom - 3.0)), {"minecraft:fall", nullptr});
		}
	}

	void respawn(Server& server, Player& player) {
		{
			CombatState& state = player.combat();
			if (!state.dead) return;
			state.dead				= false;
			state.health			= MAX_HEALTH;
			state.food				= 20;
			state.saturation		= 5;
			state.fallDistance		= 0;
			state.lastDamage		= 0;
			state.invulnerableUntil = 0;
		}
		const World::Spawn& spawn = server.getWorld().getSpawn();
		player.setPosition(spawn.x, spawn.y, spawn.z);
		player.setOnGround(true);
		std::shared_ptr<Player> self = player.shared_from_this();

		Buffer respawnPacket;
		writeSpawnInfo(respawnPacket, player, server);
		respawnPacket.writeUByte(0); // Data kept: none, like after a death
		Packet::send(self, PacketId::Play::Clientbound::RESPAWN, respawnPacket, server);

		Buffer waitChunks;
		waitChunks.writeUByte(GAME_EVENT_WAIT_CHUNKS);
		waitChunks.writeFloat(0);
		Packet::send(self, PacketId::Play::Clientbound::GAME_EVENT, waitChunks, server);

		Packet packet(self);
		synchronizePlayerPositionPacket(packet, server);
		playerAbilitiesPacket(packet, server);
		setHeldItemPacket(packet, server);
		sendHealth(server, player);

		if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->onPlayerMove(spawn.x, spawn.z);
		server.getPlayerTracker().respawn(&player);
	}

	void tick(Server& server, Player& player) {
		CombatState& state = player.combat();
		if (state.dead) return;
		int64_t now = currentTick(server);
		if (state.health < MAX_HEALTH && state.food >= 18 && now - state.lastRegeneration >= REGENERATION_TICKS) {
			state.health		   = std::min(MAX_HEALTH, state.health + 1);
			state.lastRegeneration = now;
			sendHealth(server, player);
		}
		if (state.inCombat && now - state.lastCombat > COMBAT_END_TICKS) {
			state.inCombat = false;
			Buffer end;
			end.writeVarInt(static_cast<int32_t>(now - state.combatStart)); // Duration in ticks
			Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::PLAYER_COMBAT_END, end, server);
		}
	}

} // namespace Combat
