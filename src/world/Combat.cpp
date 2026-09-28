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
#include "world/Survival.hpp"
#include "world/Level.hpp"
#include "world/World.hpp"
#include "world/entity/LivingEntity.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

namespace {
	constexpr float	  MAX_HEALTH			= 20;
	constexpr int64_t INVULNERABILITY_TICKS = 10;
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
	double reachDistance(const Player& attacker, const AABB& box) {
		double eyeX = attacker.getX(), eyeY = attacker.getY() + EYE_HEIGHT, eyeZ = attacker.getZ();
		double x = std::clamp(eyeX, box.minX, box.maxX);
		double y = std::clamp(eyeY, box.minY, box.maxY);
		double z = std::clamp(eyeZ, box.minZ, box.maxZ);
		return std::sqrt((x - eyeX) * (x - eyeX) + (y - eyeY) * (y - eyeY) + (z - eyeZ) * (z - eyeZ));
	}

	// The part of Player.attack both kinds of targets share: the held item's damage scaled by how charged the attack
	// is (spamming clicks does little), critical hits when falling. Resets the attack cooldown
	struct AttackRoll {
		float damage;
		bool  strong, critical;
	};
	std::optional<AttackRoll> rollAttack(Server& server, Player& attacker, const AABB& target) {
		if (attacker.getGameMode() == GameMode::Spectator || attacker.combat().dead) return std::nullopt;
		double fallDistance = attacker.combat().fallDistance;
		// Vanilla entity interaction range: 3 blocks, 5 in creative, plus a margin for latency
		double range = (attacker.getGameMode() == GameMode::Creative ? 5.0 : 3.0) + 1.0;
		if (reachDistance(attacker, target) > range) return std::nullopt;

		const GameData::ItemProperties* item		= server.getGameData().getItemProperties(attacker.getItemInHand(0));
		float							baseDamage	= 1.0f + (item ? item->attackDamage : 0.0f);
		float							attackSpeed = std::max(0.1f, 4.0f + (item ? item->attackSpeed : 0.0f));
		int64_t							now			= currentTick(server);
		// Vanilla getAttackStrengthScale(0.5): ticks since the last attack against the item's cooldown
		double strength = std::clamp((now - attacker.getLastAttack() + 0.5) / (20.0 / attackSpeed), 0.0, 1.0);
		attacker.setLastAttack(now);

		AttackRoll roll;
		roll.damage	  = baseDamage * static_cast<float>(0.2 + strength * strength * 0.8);
		roll.strong	  = strength > 0.9;
		roll.critical = roll.strong && fallDistance > 0 && !attacker.isOnGround() && !attacker.isSprinting();
		if (roll.critical) roll.damage *= 1.5f;
		return roll;
	}

	void sendCriticalHit(Server& server, int targetId, Player& attacker) {
		Buffer animation;
		animation.writeVarInt(targetId);
		animation.writeUByte(ANIMATE_CRITICAL_HIT);
		server.getPlayerTracker().broadcast(&attacker, PacketId::Play::Clientbound::ANIMATE, animation, true);
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

	// Whether the block at pos is still a valid respawn point: a bed or a respawn anchor (Vanilla's
	// ServerPlayer.getBedSpawnLocation), with nothing solid blocking the space above
	bool isSpawnBlock(Level& level, const BlockPos& pos) {
		const GameData& data = level.gameData();
		auto			isBedOrAnchor = [&](const BlockPos& at) {
			int state = level.getBlockState(at);
			if (state < 0) return false;
			int block = level.blocks().blockOf(state);
			return data.isInstanceOf(block, "BedBlock") || data.isInstanceOf(block, "RespawnAnchorBlock");
		};
		return (isBedOrAnchor(pos) || isBedOrAnchor(pos.below())) && !level.isRedstoneConductor(level.getBlockState(pos.above()));
	}
} // namespace

namespace Combat {

	void sendHealth(Server& server, Player& player) { Survival::sendHealth(server, player); }

	void attack(Server& server, Player& attacker, Player& target) {
		if (&attacker == &target) return;
		std::optional<AttackRoll> roll = rollAttack(server, attacker,
													AABB{target.getX() - PLAYER_HALF_WIDTH, target.getY(), target.getZ() - PLAYER_HALF_WIDTH,
														 target.getX() + PLAYER_HALF_WIDTH, target.getY() + PLAYER_HEIGHT, target.getZ() + PLAYER_HALF_WIDTH});
		if (!roll) return;
		float damage = roll->damage;
		bool  strong = roll->strong, critical = roll->critical;

		DamageSource source{"minecraft:player_attack", &attacker};
		if (!Combat::damage(server, target, damage, source)) return;
		Survival::causeFoodExhaustion(attacker, FoodData::EXHAUSTION_ATTACK); // Player.attack

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

	// Player.attack against a LivingEntity: hurtServer, then the sprint knockback along the attacker's look, the
	// critical hit animation and the attack sounds
	void attack(Server& server, Player& attacker, LivingEntity& target) {
		if (!target.isAlive()) return; // Entity.isAttackable
		std::optional<AttackRoll> roll = rollAttack(server, attacker, target.boundingBox());
		if (!roll) return;
		Level&		level  = target.level();
		bool		sprint = roll->strong && attacker.isSprinting();
		auto		sound  = [&](const char* name) {
			  level.playSoundAt(nullptr, attacker.getX(), attacker.getY(), attacker.getZ(), name, Level::SoundSource::Players, 1.0F, 1.0F);
		};
		if (sprint) sound("minecraft:entity.player.attack.knockback");
		if (!target.hurtServer({"minecraft:player_attack", &attacker}, roll->damage)) {
			sound("minecraft:entity.player.attack.nodamage");
			return;
		}
		// getKnockback: the attack_knockback attribute (0 for players without enchantments), +1 when sprinting
		float knockback = sprint ? 1.0F : 0.0F;
		if (knockback > 0.0F) {
			float yaw = attacker.getYaw() * (float)(M_PI / 180.0);
			target.knockback(knockback * 0.5F, Mth::sin(yaw), -Mth::cos(yaw));
			// The attacker slows down and stops sprinting on its side (its movement is the client's)
		}
		// Sweeping attacks aren't ported
		if (roll->critical) {
			sound("minecraft:entity.player.attack.crit");
			sendCriticalHit(server, target.id(), attacker);
		} else {
			sound(roll->strong ? "minecraft:entity.player.attack.strong" : "minecraft:entity.player.attack.weak");
		}
	}

	bool damage(Server& server, Player& victim, float amount, const DamageSource& source) {
		const GameData& gameData = server.getGameData();
		int				typeId	 = gameData.getSyncedId("minecraft:damage_type", source.type);
		bool bypassesInvulnerability = gameData.isInTag("minecraft:damage_type", "minecraft:bypasses_invulnerability", typeId);
		if ((victim.getGameMode() == GameMode::Creative || victim.getGameMode() == GameMode::Spectator) && !bypassesInvulnerability) return false;

		int64_t now	 = currentTick(server);
		Armor	armor = wornArmor(gameData, victim);
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
			if (!gameData.isInTag("minecraft:damage_type", "minecraft:bypasses_armor", typeId)) applied = afterArmor(applied, armor);
			// Player.actuallyHurt: the damage type's exhaustion
			if (applied != 0) Survival::causeFoodExhaustion(victim, gameData.getDamageExhaustion(source.type));

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
		}

		// SET_HEALTH follows at the player's tick (ServerPlayer.doTick)
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
		const GameData&		 gameData = server.getGameData();
		Level&				 level	  = server.getLevel();
		const BlockRegistry& blocks	  = level.blocks();
		int					 state	  = level.getBlockState({static_cast<int>(std::floor(player.getX())), static_cast<int>(std::floor(y)),
															 static_cast<int>(std::floor(player.getZ()))});
		int					 block	  = blocks.blockOf(state);
		bool				 cushioned = block == gameData.getStaticId("minecraft:block", "minecraft:water") ||
							 block == gameData.getStaticId("minecraft:block", "minecraft:lava") ||
							 gameData.isInTag("minecraft:block", "minecraft:climbable", block) || blocks.getBool(state, blocks.property("waterlogged"));

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
			state.fallDistance		= 0;
			state.lastDamage		= 0;
			state.invulnerableUntil = 0;
		}
		Survival::reset(player); // A new ServerPlayer: full food and air

		// The respawn position: the player's bed or respawn anchor if it is still there, the world spawn otherwise
		bool	 invalid = false;
		Vec3	 respawn = respawnPosition(server.getLevel(), {server.getWorld().getSpawn().x, server.getWorld().getSpawn().y, server.getWorld().getSpawn().z},
										  player.spawn(), invalid);
		if (invalid) server.sendSystemMessage(player, "block.minecraft.spawn.not_valid");
		player.setPosition(respawn.x, respawn.y, respawn.z);
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

		if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->onPlayerMove(respawn.x, respawn.z);
		server.getPlayerTracker().respawn(&player);
	}

	Vec3 respawnPosition(Level& level, const Vec3& fallback, const PlayerSpawn& home, bool& invalid) {
		invalid = false;
		if (home.valid && home.dimension == level.dimensionName()) {
			BlockPos at(home.x, home.y, home.z);
			if (isSpawnBlock(level, at)) return {at.x + 0.5, static_cast<double>(at.y), at.z + 0.5};
			invalid = true; // The bed or anchor is missing or obstructed
		}
		return {fallback.x, fallback.y, fallback.z};
	}

	void tick(Server& server, Player& player) {
		CombatState& state = player.combat();
		if (state.dead) return;
		int64_t now = currentTick(server); // Natural regeneration is FoodData's (Survival::tick)
		if (state.inCombat && now - state.lastCombat > COMBAT_END_TICKS) {
			state.inCombat = false;
			Buffer end;
			end.writeVarInt(static_cast<int32_t>(now - state.combatStart)); // Duration in ticks
			Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::PLAYER_COMBAT_END, end, server);
		}
	}

} // namespace Combat
