#include "world/Survival.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/PlayerTracker.hpp"
#include "world/item/ItemUse.hpp"

#include <cmath>
#include <optional>

namespace {
	constexpr float MAX_HEALTH			= 20;
	constexpr int	ENTITY_EVENT_DROWN	= 67; // EntityEvent: bubbles around a drowning entity
	constexpr int	AIR_DATA_INDEX		= 1;  // Entity.DATA_AIR_SUPPLY_ID
	constexpr int	LIVING_FLAGS_INDEX	= 8;  // LivingEntity.DATA_LIVING_ENTITY_FLAGS
	constexpr int	BYTE_SERIALIZER		= 0;  // EntityDataSerializers.BYTE
	constexpr int	INT_SERIALIZER		= 1;  // EntityDataSerializers.INT
	constexpr int	HEAD_SLOT			= 5;  // Inventory window slot of the helmet

	bool invulnerable(const Player& player) { return player.getGameMode() == GameMode::Creative || player.getGameMode() == GameMode::Spectator; }

	// Avatar.POSES: height and eye height of each pose the player takes
	double poseHeight(Pose pose) { return pose == Pose::Swimming ? 0.6F : pose == Pose::Crouching ? 1.5F : 1.8F; }
	double poseEyeHeight(Pose pose) { return pose == Pose::Swimming ? 0.4F : pose == Pose::Crouching ? 1.27F : 1.62F; }

	AABB boxAt(const Player& player, Pose pose) {
		double half = Player::BB_WIDTH / 2.0;
		return {player.getX() - half, player.getY(), player.getZ() - half, player.getX() + half, player.getY() + poseHeight(pose), player.getZ() + half};
	}

	// Entity.updateInWaterStateAndDoWaterCurrentPushing / updateFluidHeightAndDoFluidPushing(WATER): water reaches the box
	bool touchesWater(Level& level, const AABB& entityBox) {
		AABB box = entityBox.deflate(0.001);
		for (int x = Mth::floor(box.minX); x < Mth::ceil(box.maxX); x++) {
			for (int y = Mth::floor(box.minY); y < Mth::ceil(box.maxY); y++) {
				for (int z = Mth::floor(box.minZ); z < Mth::ceil(box.maxZ); z++) {
					FluidState fluid = level.getFluidState({x, y, z});
					if (level.fluids().isWater(fluid.type) && y + level.fluids().height(fluid, {x, y, z}) >= box.minY) return true;
				}
			}
		}
		return false;
	}

	// Entity.updateFluidOnEyes: the water's surface is above the eyes
	bool eyeInWater(Level& level, const Player& player) {
		double	   eyeY = player.getY() + Survival::eyeHeight(player);
		BlockPos   pos{Mth::floor(player.getX()), Mth::floor(eyeY), Mth::floor(player.getZ())};
		FluidState fluid = level.getFluidState(pos);
		return level.fluids().isWater(fluid.type) && pos.y + level.fluids().height(fluid, pos) > eyeY;
	}

	// LivingEntity.onClimbable: in a #climbable block, or an open trapdoor above a ladder facing the same way
	bool onClimbable(Level& level, const Player& player) {
		if (player.getGameMode() == GameMode::Spectator) return false;
		const GameData&		 gameData = level.gameData();
		const BlockRegistry& blocks	  = level.blocks();
		BlockPos			 pos{Mth::floor(player.getX()), Mth::floor(player.getY()), Mth::floor(player.getZ())};
		int					 state = level.getBlockState(pos);
		int					 block = blocks.blockOf(state);
		if (gameData.isInTag("minecraft:block", "minecraft:climbable", block)) return true;
		if (!gameData.isInstanceOf(block, "TrapDoorBlock") || !blocks.getBool(state, blocks.property("open"))) return false;
		int below = level.getBlockState(pos.below());
		return blocks.blockOf(below) == gameData.getStaticId("minecraft:block", "minecraft:ladder") &&
			   blocks.get(below, blocks.property("facing")) == blocks.get(state, blocks.property("facing"));
	}

	// The level of Respiration on the helmet (the OXYGEN_BONUS attribute it gives: 1 per level), 0 if none
	int respirationLevel(const GameData& gameData, const Player& player) {
		return ItemUse::enchantmentLevel(gameData, player.inventory().get(HEAD_SLOT), "minecraft:respiration");
	}

	// LivingEntity.decreaseAirSupply: with an oxygen bonus, a chance to keep the air
	int decreaseAirSupply(const GameData& gameData, Player& player, int air) {
		double bonus = respirationLevel(gameData, player);
		return bonus > 0.0 && player.random().nextDouble() >= 1.0 / (bonus + 1.0) ? air : air - 1;
	}

	// LivingEntity.increaseAirSupply
	int increaseAirSupply(int air) { return std::min(air + 4, Survival::MAX_AIR_SUPPLY); }

	// Entity.baseTick's water state and LivingEntity.baseTick's air
	void baseTick(Level& level, Player& player) {
		SurvivalState& state = player.survival();
		state.inWater		 = touchesWater(level, boxAt(player, state.pose));
		if (state.inWater) player.combat().fallDistance = 0; // resetFallDistance
		bool wasEyeInWater = state.eyeInWater;
		state.eyeInWater   = eyeInWater(level, player);
		// Entity.updateSwimming (isUnderWater: wasEyeInWater && isInWater)
		BlockPos feet{Mth::floor(player.getX()), Mth::floor(player.getY()), Mth::floor(player.getZ())};
		if (state.swimming) {
			state.swimming = player.isSprinting() && state.inWater;
		} else {
			state.swimming = player.isSprinting() && wasEyeInWater && state.inWater && level.fluids().isWater(level.getFluidState(feet).type);
		}

		// LivingEntity.baseTick: air under water (not in a bubble column), drowning at -20
		double	 eyeY = player.getY() + Survival::eyeHeight(player);
		BlockPos eye{Mth::floor(player.getX()), Mth::floor(eyeY), Mth::floor(player.getZ())};
		if (state.eyeInWater && level.blocks().blockOf(level.getBlockState(eye)) != level.bubbleColumnBlock()) {
			if (!invulnerable(player)) { // canBreatheUnderwater, water breathing: no effects yet
				player.setAirSupply(decreaseAirSupply(level.gameData(), player, player.getAirSupply()));
				if (player.getAirSupply() <= -20) { // shouldTakeDrowningDamage
					player.setAirSupply(0);
					Buffer event;
					event.writeInt(player.getPlayerID());
					event.writeByte(ENTITY_EVENT_DROWN);
					level.server().getPlayerTracker().broadcast(&player, PacketId::Play::Clientbound::ENTITY_EVENT, event, true);
					Combat::damage(level.server(), player, 2.0F, {"minecraft:drown", nullptr});
				}
			} else if (player.getAirSupply() < Survival::MAX_AIR_SUPPLY) {
				player.setAirSupply(increaseAirSupply(player.getAirSupply()));
			}
		} else if (player.getAirSupply() < Survival::MAX_AIR_SUPPLY) {
			player.setAirSupply(increaseAirSupply(player.getAirSupply()));
		}
	}

	// ServerPlayer.tickRegeneration: in peaceful, health and saturation every second, food every half second
	void tickRegeneration(Level& level, Player& player) {
		if (Survival::difficulty(level.server()) != Survival::Difficulty::Peaceful) return; // naturalRegeneration: always on
		FoodData& food = player.foodData();
		int64_t	  tick = player.survival().tickCount;
		if (tick % 20 == 0) {
			if (player.combat().health < MAX_HEALTH) Survival::heal(player, 1.0F);
			float saturation = food.getSaturationLevel();
			if (saturation < FoodData::MAX_SATURATION) food.setSaturation(saturation + 1.0F);
		}
		if (tick % 10 == 0 && food.needsFood()) food.setFoodLevel(food.getFoodLevel() + 1);
	}

	// Player.updatePlayerPose: swimming, crouching (sneaking, not flying) or standing, if the player fits
	void updatePose(Level& level, Player& player) {
		SurvivalState& state = player.survival();
		auto		   fits	 = [&](Pose pose) { return !level.hasBlockCollision(boxAt(player, pose).deflate(1.0E-7)); };
		if (!fits(Pose::Swimming)) return;
		Pose desired = state.swimming ? Pose::Swimming : player.isShiftKeyDown() && !state.flying ? Pose::Crouching : Pose::Standing;
		if (player.getGameMode() == GameMode::Spectator || fits(desired)) {
			state.pose = desired;
		} else {
			state.pose = fits(Pose::Crouching) ? Pose::Crouching : Pose::Swimming;
		}
	}

	void writeAir(Buffer& data, const Player& player) {
		data.writeUByte(AIR_DATA_INDEX);
		data.writeVarInt(INT_SERIALIZER);
		data.writeVarInt(player.getAirSupply());
	}
	void writeLivingFlags(Buffer& data, const Player& player) {
		data.writeUByte(LIVING_FLAGS_INDEX);
		data.writeVarInt(BYTE_SERIALIZER);
		data.writeUByte(player.survival().livingFlags);
	}
} // namespace

namespace Survival {

	Difficulty difficulty(Server& server) {
		const std::string name = server.getConfig().getDifficulty();
		if (name == "peaceful") return Difficulty::Peaceful;
		if (name == "easy") return Difficulty::Easy;
		if (name == "hard") return Difficulty::Hard;
		return Difficulty::Normal;
	}

	void causeFoodExhaustion(Player& player, float exhaustion) {
		if (!invulnerable(player)) player.foodData().addExhaustion(exhaustion);
	}

	void heal(Player& player, float amount) {
		CombatState& combat = player.combat();
		if (combat.health > 0.0F) combat.health = std::clamp(combat.health + amount, 0.0F, MAX_HEALTH);
	}

	bool isHurt(const Player& player) {
		float health = player.combat().health;
		return health > 0.0F && health < MAX_HEALTH;
	}

	double eyeHeight(const Player& player) { return poseEyeHeight(player.survival().pose); }

	void jumpFromGround(Player& player) {
		causeFoodExhaustion(player, player.isSprinting() ? FoodData::EXHAUSTION_SPRINT_JUMP : FoodData::EXHAUSTION_JUMP);
	}

	void checkMovementStatistics(Level& level, Player& player, double dx, double dy, double dz) {
		if (dx == 0.0 && dy == 0.0 && dz == 0.0) return; // didNotMove
		const SurvivalState& state = player.survival();
		// Distances in cm, Math.round of a float
		auto centimeters = [](double squared) { return static_cast<int>(std::lround(static_cast<float>(std::sqrt(squared)) * 100.0F)); };
		if (state.swimming || state.eyeInWater) {
			int distance = centimeters(dx * dx + dy * dy + dz * dz);
			if (distance > 0) causeFoodExhaustion(player, FoodData::EXHAUSTION_SWIM * distance * 0.01F);
		} else if (state.inWater) {
			int distance = centimeters(dx * dx + dz * dz);
			if (distance > 0) causeFoodExhaustion(player, FoodData::EXHAUSTION_SWIM * distance * 0.01F);
		} else if (onClimbable(level, player)) {
			// Climbing only counts for the statistics
		} else if (player.isOnGround()) {
			int distance = centimeters(dx * dx + dz * dz);
			// Walking and crouching cost nothing (EXHAUSTION_WALK, EXHAUSTION_CROUCH are 0)
			if (distance > 0 && player.isSprinting()) causeFoodExhaustion(player, FoodData::EXHAUSTION_SPRINT * distance * 0.01F);
		}
	}

	void onMove(Level& level, Player& player, double dx, double dy, double dz, bool packetOnGround) {
		if (player.isOnGround() && !packetOnGround && dy > 0.0) jumpFromGround(player);
		player.setOnGround(packetOnGround);
		checkMovementStatistics(level, player, dx, dy, dz);
	}

	void tickFood(Level& level, Player& player) {
		FoodData&  food		  = player.foodData();
		Difficulty difficulty = Survival::difficulty(level.server());
		if (food.getExhaustionLevel() > FoodData::EXHAUSTION_DROP) {
			food.setExhaustion(food.getExhaustionLevel() - FoodData::EXHAUSTION_DROP);
			if (food.getSaturationLevel() > 0.0F) {
				food.setSaturation(std::max(food.getSaturationLevel() - 1.0F, 0.0F));
			} else if (difficulty != Difficulty::Peaceful) {
				food.setFoodLevel(std::max(food.getFoodLevel() - 1, 0));
			}
		}
		// The naturalRegeneration game rule isn't there yet: always on
		if (food.getSaturationLevel() > 0.0F && isHurt(player) && food.getFoodLevel() >= FoodData::MAX_FOOD) {
			food.setTickTimer(food.getTickTimer() + 1);
			if (food.getTickTimer() >= 10) {
				float saturation = std::min(food.getSaturationLevel(), 6.0F);
				heal(player, saturation / 6.0F);
				food.addExhaustion(saturation);
				food.setTickTimer(0);
			}
		} else if (food.getFoodLevel() >= FoodData::HEAL_LEVEL && isHurt(player)) {
			food.setTickTimer(food.getTickTimer() + 1);
			if (food.getTickTimer() >= 80) {
				heal(player, 1.0F);
				food.addExhaustion(FoodData::EXHAUSTION_HEAL);
				food.setTickTimer(0);
			}
		} else if (food.getFoodLevel() <= 0) {
			food.setTickTimer(food.getTickTimer() + 1);
			if (food.getTickTimer() >= 80) {
				float health = player.combat().health;
				if (health > 10.0F || difficulty == Difficulty::Hard || (health > 1.0F && difficulty == Difficulty::Normal)) {
					Combat::damage(level.server(), player, 1.0F, {"minecraft:starve", nullptr});
				}
				food.setTickTimer(0);
			}
		} else {
			food.setTickTimer(0);
		}
	}

	void tick(Level& level, Player& player) {
		SurvivalState& state = player.survival();
		state.tickCount++;
		if (!player.combat().dead) {
			// Player.tick: LivingEntity.baseTick, LivingEntity.tick's updatingUsingItem, aiStep's tickRegeneration, the pose
			baseTick(level, player);
			if (!player.combat().dead) ItemUse::updatingUsingItem(level, player);
			if (!player.combat().dead) tickRegeneration(level, player);
			updatePose(level, player);
			// ServerPlayer.doTick
			if (!player.combat().dead) tickFood(level, player);
		}
		const FoodData& food = player.foodData();
		if (player.combat().health != state.lastSentHealth || state.lastSentFood != food.getFoodLevel() ||
			(food.getSaturationLevel() == 0.0F) != state.lastFoodSaturationZero) {
			sendHealth(level.server(), player);
		}
	}

	void sendHealth(Server& server, Player& player) {
		SurvivalState&	state = player.survival();
		const FoodData& food  = player.foodData();
		Buffer			buf;
		buf.writeFloat(player.combat().health);
		buf.writeVarInt(food.getFoodLevel());
		buf.writeFloat(food.getSaturationLevel());
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::SET_HEALTH, buf, server);
		state.lastSentHealth		 = player.combat().health;
		state.lastSentFood			 = food.getFoodLevel();
		state.lastFoodSaturationZero = food.getSaturationLevel() == 0.0F;
	}

	void sendDirtyData(Server& server, Player& player) {
		SurvivalState& state = player.survival();
		if (!state.dirtyData) return;
		Buffer data;
		data.writeVarInt(player.getPlayerID());
		if (state.dirtyData & DATA_AIR_SUPPLY) writeAir(data, player);
		if (state.dirtyData & DATA_LIVING_FLAGS) writeLivingFlags(data, player);
		data.writeUByte(0xFF); // End of the entity data
		state.dirtyData = 0;
		// ServerEntity.sendDirtyEntityData: to the viewers and the player itself
		server.getPlayerTracker().broadcast(&player, PacketId::Play::Clientbound::SET_ENTITY_DATA, data, true);
	}

	bool writeNonDefaultData(Buffer& data, const Player& player) {
		bool air = player.getAirSupply() != MAX_AIR_SUPPLY, flags = player.survival().livingFlags != 0;
		if (!air && !flags) return false;
		data.writeVarInt(player.getPlayerID());
		if (air) writeAir(data, player);
		if (flags) writeLivingFlags(data, player);
		data.writeUByte(0xFF);
		return true;
	}

	void reset(Player& player) {
		SurvivalState& state = player.survival();
		player.foodData()	 = FoodData();
		player.setAirSupply(MAX_AIR_SUPPLY);
		ItemUse::stopUsingItem(player);
		state.pose		   = Pose::Standing;
		state.inWater	   = false;
		state.eyeInWater   = false;
		state.swimming	   = false;
		state.lastSentHealth = -1.0F;
		state.lastSentFood	 = -1;
	}

} // namespace Survival
