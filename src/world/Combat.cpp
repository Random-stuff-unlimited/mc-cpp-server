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
#include "world/blocks/BlockContext.hpp"
#include "world/World.hpp"
#include "world/entity/DismountHelper.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/item/Enchantments.hpp"
#include "world/inventory/Menu.hpp"
#include "world/Xp.hpp"

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
	constexpr int	  GAME_EVENT_NO_RESPAWN_BLOCK = 0;
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

	// Player.dropEquipment (keepInventory isn't a game rule here: always off): the items cursed with vanishing are
	// destroyed, then everything is thrown around (Inventory.dropAll: hotbar and main, then the equipment in
	// EquipmentSlot order: offhand, feet, legs, chest, head)
	void dropInventory(Server& server, Player& victim) {
		if (victim.isSpectator()) return;
		Level&			 level = server.levelOf(victim);
		const GameData&	 data  = server.getGameData();
		PlayerInventory& inventory = victim.inventory();
		auto			 vanishes  = [&](const ItemStack& stack) {
			  for (const auto& [enchantment, lvl] : Enchantments::of(data, stack)) {
				  const nlohmann::json* definition = Enchantments::definition(data, enchantment);
				  if (definition && definition->contains("effects") && definition->at("effects").contains("minecraft:prevent_equipment_drop")) return true;
			  }
			  return false;
		};
		for (int slot = 0; slot < PlayerInventory::SIZE; slot++) {
			if (!inventory.get(slot).isEmpty() && vanishes(inventory.get(slot))) inventory.set(slot, ItemStack{});
		}
		std::vector<int> order;
		for (int i = 0; i < 36; i++) order.push_back(PlayerInventory::windowSlot(i));
		for (int slot : {PlayerInventory::OFFHAND, 8, 7, 6, 5}) order.push_back(slot);
		for (int slot : order) {
			ItemStack stack = inventory.get(slot);
			if (stack.isEmpty()) continue;
			inventory.set(slot, ItemStack{});
			level.dropRandomlyFromPlayer(victim, std::move(stack));
		}
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
		std::string		   sourceName = source.causing ? Combat::displayName(server.getGameData(), *source.causing) : "";
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

		// LivingEntity.dropAllDeathLoot, after the message
		dropInventory(server, victim);
		// Player.dropExperience: half the total, at most 100, drops as orbs around the body
		if (victim.getGameMode() != GameMode::Spectator) {
			int xp = std::min(victim.getXpTotal() / 2, 100);
			if (xp > 0) {
				Level& level = server.levelOf(victim);
				for (int dropped = 0; dropped < xp;) {
					int split = std::min(20, xp - dropped);
					dropped += split;
					if (auto orb = ExperienceOrb::create(level, {victim.getX(), victim.getY() + 0.5, victim.getZ()}, split))
						level.entities().add(std::move(orb));
				}
				Xp::addExperience(server, victim, -xp);
			}
		}
	}

} // namespace

namespace Combat {

	void sendHealth(Server& server, Player& player) { Survival::sendHealth(server, player); }

	std::string displayName(const GameData& gameData, const Actor& actor) {
		if (const Player* player = actor.asPlayer()) return const_cast<Player*>(player)->getPlayerName();
		std::string name = gameData.getStaticName("minecraft:entity_type", actor.typeId());
		if (size_t colon = name.find(':'); colon != std::string::npos) name = name.substr(colon + 1);
		bool start = true;
		for (char& c : name) {
			if (c == '_') {
				c	  = ' ';
				start = true;
			} else if (start) {
				c	  = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				start = false;
			}
		}
		return name;
	}

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
		// Damage wakes a sleeping player up (LivingEntity.hurt stops the sleep first)
		if (victim.survival().sleeping) server.wakeUp(victim);

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
			if (Player* attacker = source.attackerPlayer()) {
				state.lastAttacker	 = attacker->getPlayerName();
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
				state.deathDimension   = victim.level() ? victim.level()->dimensionName() : "minecraft:overworld";
				died				   = true;
			} else if (source.causing) {
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
		event.writeVarInt(source.causing ? source.causing->id() + 1 : 0);				 // Cause
		event.writeVarInt(source.directEntity() ? source.directEntity()->id() + 1 : 0); // Direct (a projectile...)
		event.writeBool(source.position.has_value());									 // sourcePositionRaw
		if (source.position) {
			event.writeDouble(source.position->x);
			event.writeDouble(source.position->y);
			event.writeDouble(source.position->z);
		}
		server.getPlayerTracker().broadcast(&victim, PacketId::Play::Clientbound::DAMAGE_EVENT, event, true);

		std::optional<Vec3> from = source.sourcePosition();
		if (from && source.causing) {
			// Knockback away from the attacker, and the victim's camera tilts toward the hit
			double dx = from->x - victim.getX();
			double dz = from->z - victim.getZ();
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
		Level& level = player.level() ? *player.level() : server.getLevel();
		double y	 = player.getY();

		// Below the world: 4 damage every half second, even in creative
		if (y < level.minY() - 64) {
			damage(server, player, 4.0f, {"minecraft:out_of_world", nullptr});
			return;
		}
		if (player.getGameMode() == GameMode::Creative || player.getGameMode() == GameMode::Spectator) {
			player.combat().fallDistance = 0;
			return;
		}

		// Water, lava, ladders and vines stop a fall
		const GameData&		 gameData = server.getGameData();
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

	void respawn(Server& server, Player& player, bool keepAllData) {
		if (!keepAllData) {
			CombatState& state = player.combat();
			if (!state.dead) return;
			state.dead				= false;
			state.health			= MAX_HEALTH;
			state.fallDistance		= 0;
			state.lastDamage		= 0;
			state.invulnerableUntil = 0;
			Survival::reset(player); // A new ServerPlayer: full food and air
			// Its inventory is new too (keepInventory isn't a game rule here): what came back into the old one during
			// the death screen (the crafting grid, the cursor) is gone
			for (int slot = 0; slot < PlayerInventory::SIZE; slot++) player.inventory().set(slot, ItemStack{});
			if (player.inventoryMenuSlot()) player.inventoryMenuSlot()->carried() = ItemStack{};
		}

		// ServerPlayer.findRespawnPositionAndUseSpawnBlock: the bed or respawn anchor (using a charge) in its dimension if
		// it is still there, else the world spawn in the overworld (and the respawn point is forgotten)
		Level*					  destination = &server.getLevel();
		const World::Spawn&		  worldSpawn  = server.getWorld().getSpawn();
		Vec3					  position{worldSpawn.x, worldSpawn.y, worldSpawn.z};
		float					  yaw = 0.0F, pitch = 0.0F;
		bool					  missingBlock = false;
		PlayerSpawn&			  home		   = player.spawn();
		Level*					  homeLevel	   = home.valid ? server.getLevel(home.dimension) : nullptr;
		if (homeLevel) {
			if (std::optional<RespawnPos> found = findRespawnAndUseSpawnBlock(*homeLevel, home, !keepAllData)) {
				destination = homeLevel;
				position	= found->position;
				yaw			= found->yaw;
				pitch		= found->pitch;
			} else {
				missingBlock = true;
				home		 = {}; // Not copied to the respawned player
			}
		}
		std::shared_ptr<Player> self	 = player.shared_from_this();
		Level*					previous = player.level();
		// PlayerList.respawn: the player leaves its level for the respawn dimension (already gone after the credits)
		bool rejoin = previous != destination || keepAllData;
		if (rejoin) {
			if (ChunkStreamer* streamer = player.getChunkStreamer()) streamer->stop();
			if (previous) previous->removePlayer(&player);
			player.setLevel(destination);
		}
		player.setPosition(position.x, position.y, position.z);
		player.setRotation(yaw, pitch);
		player.setOnGround(true);
		if (missingBlock) {
			Buffer event;
			event.writeUByte(GAME_EVENT_NO_RESPAWN_BLOCK);
			event.writeFloat(0);
			Packet::send(self, PacketId::Play::Clientbound::GAME_EVENT, event, server);
		}

		Buffer respawnPacket;
		writeSpawnInfo(respawnPacket, player, server, *destination);
		respawnPacket.writeUByte(keepAllData ? 1 : 0); // Data kept: the attributes after the credits, nothing after a death
		Packet::send(self, PacketId::Play::Clientbound::RESPAWN, respawnPacket, server);

		Packet packet(self);
		synchronizePlayerPositionPacket(packet, server);
		server.sendDefaultSpawn(self);
		changeDifficultyPacket(packet, server);
		server.sendLevelInfo(self, *destination);
		playerAbilitiesPacket(packet, server);
		setHeldItemPacket(packet, server);
		sendHealth(server, player);
		Xp::send(server, player); // The client's bar reset on respawn, set back to its value
		if (rejoin) destination->addPlayer(self);

		// The anchor lost a charge: its sound, to this player only
		if (!keepAllData && homeLevel && !missingBlock && homeLevel->blocks().blockOf(homeLevel->getBlockState({home.x, home.y, home.z})) ==
												  server.getGameData().getStaticId("minecraft:block", "minecraft:respawn_anchor")) {
			Buffer sound;
			int	   id = server.getGameData().getStaticId("minecraft:sound_event", "minecraft:block.respawn_anchor.deplete");
			sound.writeVarInt(id + 1);
			sound.writeVarInt(static_cast<int>(Level::SoundSource::Blocks));
			sound.writeInt(home.x * 8);
			sound.writeInt(home.y * 8);
			sound.writeInt(home.z * 8);
			sound.writeFloat(1.0F);
			sound.writeFloat(1.0F);
			sound.writeLong(static_cast<int64_t>(destination->random().nextLong()));
			Packet::send(self, PacketId::Play::Clientbound::SOUND, sound, server);
		}

		if (ChunkStreamer* streamer = player.getChunkStreamer()) {
			if (rejoin) {
				streamer->start(position.x, position.z, streamer->viewDistance());
			} else {
				streamer->onPlayerMove(position.x, position.z);
			}
		}
		server.getPlayerTracker().respawn(&player);
	}

	std::optional<RespawnPos> findRespawnAndUseSpawnBlock(Level& level, const PlayerSpawn& home, bool useCharge) {
		if (!home.valid) return std::nullopt;
		BlockPos					   pos(home.x, home.y, home.z);
		int							   state  = level.getBlockState(pos);
		int							   block  = level.blocks().blockOf(state);
		const GameData&				   data	  = level.gameData();
		const GameData::Dimension&	   type	  = level.dimensionType();
		// RespawnPosAngle.of: facing the block
		auto lookingAt = [&](const Vec3& at) {
			Vec3 toward = Vec3{pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5} - at;
			toward		= toward.normalize();
			return RespawnPos{at, Mth::wrapDegrees(static_cast<float>(Mth::atan2(toward.z, toward.x) * 180.0F / (float)M_PI - 90.0)), 0.0F};
		};
		if (data.isInstanceOf(block, "RespawnAnchorBlock") && (home.forced || level.blocks().get(state, level.blocks().property("charges")) > 0) &&
			type.respawnAnchorWorks) {
			// RespawnAnchorBlock.findStandUpPosition: around the anchor, safe spots first
			static constexpr int HORIZONTAL[8][3] = {{0, 0, -1}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {-1, 0, -1}, {1, 0, -1}, {-1, 0, 1}, {1, 0, 1}};
			std::vector<BlockPos> offsets;
			for (int dy : {0, -1, 1}) {
				for (const auto& o : HORIZONTAL) offsets.push_back(pos.offset(o[0], o[1] + dy, o[2]));
			}
			offsets.push_back(pos.above());
			std::optional<Vec3> found;
			for (bool safe : {true, false}) {
				for (const BlockPos& at : offsets) {
					found = DismountHelper::findSafeDismountLocation(level, Player::BB_WIDTH, Player::BB_HEIGHT, false, true, at, safe);
					if (found) break;
				}
				if (found) break;
			}
			if (!found) return std::nullopt;
			if (!home.forced && useCharge) {
				int charges = level.blocks().get(state, level.blocks().property("charges"));
				level.setBlock(pos, level.blocks().with(state, level.blocks().property("charges"), charges - 1), Level::UPDATE_ALL);
			}
			return lookingAt(*found);
		}
		if (data.isInstanceOf(block, "BedBlock") && type.bedWorks) {
			if (std::optional<Vec3> found = findBedStandUpPosition(level, pos, level.blockContext().direction(state, level.blockContext().facing), 0.0F)) {
				return lookingAt(*found);
			}
			return std::nullopt;
		}
		if (!home.forced) return std::nullopt;
		// Set by /spawnpoint: the position itself, if the player fits (Block.isPossibleToRespawnInThis)
		auto possible = [&](const BlockPos& at) {
			int s = level.getBlockState(at);
			return !data.getStateProperties(s).solid && !data.getStateProperties(s).liquid;
		};
		if (possible(pos) && possible(pos.above())) return RespawnPos{{pos.x + 0.5, pos.y + 0.1, pos.z + 0.5}, 0.0F, 0.0F};
		return std::nullopt;
	}

	std::optional<Vec3> findBedStandUpPosition(Level& level, const BlockPos& pos, Direction facing, float yaw) {
		// BedBlock.findStandUpPosition: the side the player looks away from first
		Direction side = Directions::clockWise(facing);
		// Direction.isFacingAngle: within 90 degrees of the yaw
		float	  sideYaw = Directions::toYRot(side);
		Direction dir	  = std::abs(Mth::wrapDegrees(yaw - sideYaw)) < 90.0F ? Directions::opposite(side) : side;
		int		  fx = Directions::stepX(facing), fz = Directions::stepZ(facing), dx = Directions::stepX(dir), dz = Directions::stepZ(dir);
		std::vector<std::array<int, 2>> surround = {{dx, dz},			  {dx - fx, dz - fz},		  {dx - fx * 2, dz - fz * 2},
													{-fx * 2, -fz * 2},	  {-dx - fx * 2, -dz - fz * 2}, {-dx - fx, -dz - fz},
													{-dx, -dz},			  {-dx + fx, -dz + fz},		  {fx, fz},
													{dx + fx, dz + fz}};
		std::vector<std::array<int, 2>> above = {{0, 0}, {-fx, -fz}};
		auto							atOffsets = [&](const BlockPos& from, const std::vector<std::array<int, 2>>& offsets, bool safe) -> std::optional<Vec3> {
			   for (const auto& o : offsets) {
				   if (auto found = DismountHelper::findSafeDismountLocation(level, Player::BB_WIDTH, Player::BB_HEIGHT, false, true,
																			 {from.x + o[0], from.y, from.z + o[1]}, safe)) {
					   return found;
				   }
			   }
			   return std::nullopt;
		};
		// BedBlock.isBunkBed: another bed right below
		bool bunk = level.gameData().isInstanceOf(level.blocks().blockOf(level.getBlockState(pos.below())), "BedBlock");
		if (!bunk) {
			std::vector<std::array<int, 2>> all = surround;
			all.insert(all.end(), above.begin(), above.end());
			if (auto found = atOffsets(pos, all, true)) return found;
			return atOffsets(pos, all, false);
		}
		if (auto found = atOffsets(pos, surround, true)) return found;
		if (auto found = atOffsets(pos.below(), surround, true)) return found;
		if (auto found = atOffsets(pos, above, true)) return found;
		if (auto found = atOffsets(pos, surround, false)) return found;
		if (auto found = atOffsets(pos.below(), surround, false)) return found;
		return atOffsets(pos, above, false);
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
