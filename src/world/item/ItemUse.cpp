#include "world/item/ItemUse.hpp"

#include "PacketIds.hpp"
#include "data/GameData.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/Survival.hpp"
#include "world/inventory/Menu.hpp"
#include "world/item/Components.hpp"

#include <cmath>
#include <limits>
#include <optional>

namespace {
	using Result = ItemUse::Result;
	using GD	 = GameData;

	constexpr int ENTITY_EVENT_USE_ITEM_COMPLETE = 9; // EntityEvent.USE_ITEM_COMPLETE: the client finishes the item too
	constexpr int SPYGLASS_USE_DURATION			 = 1200;
	constexpr int BLOCKING_USE_DURATION			 = 72000; // Shields, bows, crossbows, tridents

	// InteractionResult with its heldItemTransformedTo
	struct Outcome {
		Result					 result = Result::Pass;
		std::optional<ItemStack> transformed;
	};

	bool infiniteMaterials(const Player& player) { return player.getGameMode() == GameMode::Creative; }
	bool invulnerable(const Player& player) { return player.getGameMode() == GameMode::Creative || player.getGameMode() == GameMode::Spectator; }

	const std::string& itemName(const GameData& gameData, const ItemStack& stack) { return gameData.getStaticName("minecraft:item", stack.item); }

	void setItemInHand(Player& player, int hand, ItemStack stack) {
		player.inventory().set(player.handSlot(hand), stack.isEmpty() ? ItemStack() : std::move(stack));
	}

	// Player.playSound: at the player, for the others (its own client played it already)
	void playerSound(Level& level, Player& player, const std::string& sound, float volume, float pitch) {
		level.playSoundAt(&player, player.getX(), player.getY(), player.getZ(), sound, Level::SoundSource::Players, volume, pitch);
	}

	// RandomSource.triangle and Mth.randomBetween, float versions
	float triangle(JavaRandom& random, float mode, float deviation) {
		float a = random.nextFloat();
		return mode + deviation * (a - random.nextFloat());
	}
	float randomBetween(JavaRandom& random, float min, float max) { return random.nextFloat() * (max - min) + min; }

	// Inventory.contains(ItemStack): the same item and components somewhere in the inventory
	bool inventoryContains(const Player& player, const ItemStack& stack) {
		for (int slot = 5; slot < PlayerInventory::SIZE; slot++) {
			if (player.inventory().get(slot).sameItemSameComponents(stack)) return true;
		}
		return false;
	}

	// ServerPlayer.handleExtraItemsCreatedOnUse / Player.addItem: into the inventory, else dropped
	void addOrDrop(Level& level, Player& player, ItemStack stack) {
		if (!player.inventory().add(stack, player.getSelectedSlot(), infiniteMaterials(player), level.gameData())) {
			level.dropFromPlayer(player, std::move(stack), false);
		}
	}

	// ItemUtils.createFilledResult (limitCreativeStackSize true): one of `stack` becomes `result`
	ItemStack createFilledResult(Level& level, Player& player, ItemStack stack, ItemStack result) {
		if (infiniteMaterials(player)) {
			if (!inventoryContains(player, result)) addOrDrop(level, player, std::move(result));
			return stack;
		}
		stack.shrink(1);
		if (stack.isEmpty()) return result;
		addOrDrop(level, player, std::move(result));
		return stack;
	}

	// ItemStack.applyAfterUseComponentSideEffects: use_remainder (use_cooldown isn't there yet). `before` is the stack
	// as it was before being used
	ItemStack applyAfterUse(Level& level, Player& player, ItemStack stack, const ItemStack& before) {
		const GD::ItemProperties* props = level.gameData().getItemProperties(before.item);
		if (!props || props->useRemainder.empty()) return stack;
		// UseRemainder.convertIntoRemainder
		if (infiniteMaterials(player) || stack.count >= before.count) return stack;
		ItemStack remainder(level.gameData().getStaticId("minecraft:item", props->useRemainder), props->useRemainderCount);
		if (stack.isEmpty()) return remainder;
		addOrDrop(level, player, std::move(remainder));
		return stack;
	}

	// ----- Consumable -----

	// Consumable.emitParticlesAndSounds. The particles are the clients' own (ServerLevel.addParticle does nothing), but
	// they still take their random numbers
	void emitParticlesAndSounds(Level& level, Player& player, const GD::Consumable& consumable, int particles) {
		JavaRandom& random = player.random();
		float		volume = random.nextBoolean() ? 0.5F : 1.0F;
		float		pitch  = triangle(random, 1.0F, 0.2F);
		float		drinkPitch = randomBetween(random, 0.9F, 1.0F);
		bool		drink  = consumable.animation == GD::UseAnimation::Drink;
		if (consumable.hasConsumeParticles) {
			for (int i = 0; i < particles * 3; i++) random.nextFloat(); // LivingEntity.spawnItemParticles
		}
		playerSound(level, player, consumable.sound, drink ? 0.5F : volume, drink ? drinkPitch : pitch);
	}

	// Consumable.onConsume: sounds, food (FoodProperties.onConsume), effects, one item used
	ItemStack onConsume(Level& level, Player& player, ItemStack stack, const GD::ItemProperties& props) {
		const GD::Consumable& consumable = *props.consumable;
		emitParticlesAndSounds(level, player, consumable, 16);
		if (props.food) {
			JavaRandom& random = player.random();
			level.playSoundAt(nullptr, player.getX(), player.getY(), player.getZ(), consumable.sound, Level::SoundSource::Neutral, 1.0F,
							  triangle(random, 1.0F, 0.4F));
			player.foodData().eat(props.food->nutrition, props.food->saturation);
			level.playSoundAt(nullptr, player.getX(), player.getY(), player.getZ(), "minecraft:entity.player.burp", Level::SoundSource::Players, 0.5F,
							  randomBetween(random, 0.9F, 1.0F));
		}
		// Mob effects aren't there yet: apply_effects, remove_effects, clear_all_effects (milk, honey, golden apples,
		// suspicious stew...) do nothing, nor teleport_randomly (chorus fruit)
		for (const GD::ConsumeEffect& effect : consumable.onConsumeEffects) {
			if (effect.type == "minecraft:play_sound" && !effect.sound.empty()) {
				// PlaySoundConsumeEffect: at the center of the block the player is in
				level.playSound(nullptr, {Mth::floor(player.getX()), Mth::floor(player.getY()), Mth::floor(player.getZ())}, effect.sound,
								Level::SoundSource::Players);
			}
		}
		if (!infiniteMaterials(player)) stack.shrink(1); // ItemStack.consume
		return stack;
	}

	// Consumable.startConsuming
	Outcome startConsuming(Level& level, Player& player, int hand, const ItemStack& stack, const GD::ItemProperties& props) {
		// Consumable.canConsume / Player.canEat
		if (props.food && !(invulnerable(player) || props.food->canAlwaysEat || player.foodData().needsFood())) return {Result::Fail, {}};
		if (props.consumable->consumeTicks() > 0) {
			ItemUse::startUsingItem(player, hand, level.gameData());
			return {Result::Consume, {}};
		}
		return {Result::Consume, onConsume(level, player, stack, props)};
	}

	// ----- Equippable -----

	// Window slot of an equipment slot, -1 for the ones players don't have (Player.canUseSlot)
	int equipmentWindowSlot(const Player& player, GD::EquipmentSlot slot) {
		switch (slot) {
		case GD::EquipmentSlot::MainHand: return player.handSlot(0);
		case GD::EquipmentSlot::OffHand: return PlayerInventory::OFFHAND;
		case GD::EquipmentSlot::Head: return 5;
		case GD::EquipmentSlot::Chest: return 6;
		case GD::EquipmentSlot::Legs: return 7;
		case GD::EquipmentSlot::Feet: return 8;
		default: return -1;
		}
	}

	// LivingEntity.setItemSlot and onEquipItem: the equip sound of what is put on, for everyone
	void setItemSlot(Level& level, Player& player, GD::EquipmentSlot slot, ItemStack stack) {
		int		  window = equipmentWindowSlot(player, slot);
		ItemStack old	 = player.inventory().get(window);
		player.inventory().set(window, stack);
		if (player.getGameMode() == GameMode::Spectator || old.sameItemSameComponents(stack)) return;
		const GD::ItemProperties* props = level.gameData().getItemProperties(stack.item);
		if (!stack.isEmpty() && props && !props->equipSound.empty() && props->equipmentSlot == slot) {
			player.random().nextLong(); // The sound's seed (playSeededSound)
			level.playSoundAt(nullptr, player.getX(), player.getY(), player.getZ(), props->equipSound, Level::SoundSource::Players);
		}
	}

	// Equippable.swapWithEquipmentSlot
	Outcome swapWithEquipmentSlot(Level& level, Player& player, ItemStack stack, const GD::ItemProperties& props) {
		int window = equipmentWindowSlot(player, props.equipmentSlot);
		if (window < 0) return {Result::Pass, {}};
		ItemStack worn	   = player.inventory().get(window);
		bool	  creative = player.getGameMode() == GameMode::Creative;
		// Curse of binding (EnchantmentEffectComponents.PREVENT_ARMOR_CHANGE) keeps it on, except in creative
		if ((ItemUse::enchantmentLevel(level.gameData(), worn, "minecraft:binding_curse") > 0 && !creative) || stack.sameItemSameComponents(worn)) {
			return {Result::Fail, {}};
		}
		if (stack.count <= 1) {
			ItemStack inHand = worn.isEmpty() ? (creative ? stack : ItemStack()) : worn;
			setItemSlot(level, player, props.equipmentSlot, stack);
			return {Result::Success, inHand};
		}
		ItemStack one = stack.copyWithCount(1); // consumeAndReturn: the stack keeps them all with infinite materials
		if (!infiniteMaterials(player)) stack.shrink(1);
		setItemSlot(level, player, props.equipmentSlot, one);
		if (!worn.isEmpty()) addOrDrop(level, player, std::move(worn));
		return {Result::Success, stack};
	}

	// ----- BucketItem -----

	// instanceof LiquidBlockContainer. The extracted classes don't list the interfaces' own super-interfaces:
	// SimpleWaterloggedBlock extends LiquidBlockContainer (and BucketPickup)
	bool isLiquidBlockContainer(const GameData& gd, int block) {
		return gd.isInstanceOf(block, "LiquidBlockContainer") || gd.isInstanceOf(block, "SimpleWaterloggedBlock");
	}

	// BucketPickup.pickupBlock (LiquidBlock, SimpleWaterloggedBlock, PowderSnowBlock): the filled bucket, empty if
	// nothing could be taken. `sound` gets BucketPickup.getPickupSound
	ItemStack pickupBlock(Level& level, const BlockPos& pos, std::string& sound) {
		const GameData&		 gd		= level.gameData();
		const BlockRegistry& blocks = level.blocks();
		int					 state	= level.getBlockState(pos);
		int					 block	= blocks.blockOf(state);
		auto				 item	= [&](const char* name) { return ItemStack(gd.getStaticId("minecraft:item", name), 1); };
		if (gd.isInstanceOf(block, "LiquidBlock")) {
			if (blocks.getInt(state, blocks.property("level")) != 0) return {};
			bool water = level.fluids().isWater(level.getFluidState(pos).type);
			level.setBlock(pos, gd.getDefaultBlockState("minecraft:air"), Level::UPDATE_ALL_IMMEDIATE);
			sound = water ? "minecraft:item.bucket.fill" : "minecraft:item.bucket.fill_lava";
			return item(water ? "minecraft:water_bucket" : "minecraft:lava_bucket");
		}
		if (gd.isInstanceOf(block, "SimpleWaterloggedBlock")) {
			int waterlogged = blocks.property("waterlogged");
			if (!blocks.has(state, waterlogged) || !blocks.getBool(state, waterlogged)) return {};
			int dry = blocks.withBool(state, waterlogged, false);
			level.setBlock(pos, dry, Level::UPDATE_ALL);
			if (!level.behavior(dry).canSurvive(level, pos, dry)) level.destroyBlock(pos, true);
			sound = "minecraft:item.bucket.fill";
			return item("minecraft:water_bucket");
		}
		if (gd.isInstanceOf(block, "PowderSnowBlock")) {
			level.setBlock(pos, gd.getDefaultBlockState("minecraft:air"), Level::UPDATE_ALL_IMMEDIATE);
			sound = "minecraft:item.bucket.fill_powder_snow";
			return item("minecraft:powder_snow_bucket");
		}
		return {};
	}

	// BucketItem.emptyContents: the fluid poured at pos (or next to the hit block when pos can't take it)
	bool emptyContents(Level& level, Player& player, int fluid, const BlockPos& pos, const ItemUse::HitResult* hit) {
		const GameData&					  gd		 = level.gameData();
		const BlockRegistry&			  blocks	 = level.blocks();
		int								  state		 = level.getBlockState(pos);
		int								  block		 = blocks.blockOf(state);
		const GD::StateProperties&		  properties = gd.getStateProperties(state);
		bool							  water		 = fluid == level.fluids().water();
		bool							  replaceable = properties.replaceable || !properties.solid; // canBeReplaced(fluid)
		bool							  container	 = isLiquidBlockContainer(gd, block);
		bool							  placeable	 = replaceable || (container && level.fluids().canPlaceLiquid(state, fluid));
		bool							  sneaking	 = player.isShiftKeyDown();
		if (!(blocks.isAir(state) || (placeable && (!sneaking || !hit)))) {
			return hit && emptyContents(level, player, fluid, hit->pos.relative(hit->face), nullptr);
		}
		if (level.isUltraWarm() && water) {
			JavaRandom& random = level.random();
			float		a	   = random.nextFloat();
			level.playSound(&player, pos, "minecraft:block.fire.extinguish", Level::SoundSource::Blocks, 0.5F, 2.6F + (a - random.nextFloat()) * 0.8F);
			return true;
		}
		const char* sound = water ? "minecraft:item.bucket.empty" : "minecraft:item.bucket.empty_lava";
		if (container && water) {
			// LiquidBlockContainer.placeLiquid (SimpleWaterloggedBlock's; the others take nothing)
			int waterlogged = blocks.property("waterlogged");
			if (gd.isInstanceOf(block, "SimpleWaterloggedBlock") && blocks.has(state, waterlogged) && !blocks.getBool(state, waterlogged)) {
				level.setBlock(pos, blocks.withBool(state, waterlogged, true), Level::UPDATE_ALL);
				level.scheduleFluidTick(pos, fluid, level.fluids().tickDelay(fluid));
			}
			level.playSound(&player, pos, sound, Level::SoundSource::Blocks);
			return true;
		}
		if (replaceable && !properties.liquid) level.destroyBlock(pos, true);
		int source = level.fluids().legacyBlock({fluid, 8, false});
		if (!level.setBlock(pos, source, Level::UPDATE_ALL_IMMEDIATE) && !level.fluids().isSource(level.fluids().stateOf(state))) return false;
		level.playSound(&player, pos, sound, Level::SoundSource::Blocks);
		return true;
	}

	// BucketItem.use. fluid: 0 for the empty bucket
	Outcome useBucket(Level& level, Player& player, const ItemStack& stack, int fluid) {
		const GameData&	  gd	= level.gameData();
		ItemUse::HitResult hit	= ItemUse::playerPOVHitResult(level, player, fluid == 0);
		if (!hit.hit) return {Result::Pass, {}};
		// Player.mayUseItemAt: adventure mode can't build (can_place_on isn't read)
		if (player.getGameMode() == GameMode::Adventure) return {Result::Fail, {}};
		if (fluid == 0) {
			std::string sound;
			ItemStack	filled = pickupBlock(level, hit.pos, sound);
			if (filled.isEmpty()) return {Result::Fail, {}};
			playerSound(level, player, sound, 1.0F, 1.0F);
			return {Result::Success, createFilledResult(level, player, stack, std::move(filled))};
		}
		int		 block	= level.blocks().blockOf(level.getBlockState(hit.pos));
		BlockPos target = isLiquidBlockContainer(gd, block) && fluid == level.fluids().water() ? hit.pos : hit.pos.relative(hit.face);
		if (!emptyContents(level, player, fluid, target, &hit)) return {Result::Fail, {}};
		// getEmptySuccessItem: the bucket stays full with infinite materials
		ItemStack empty = infiniteMaterials(player) ? stack : ItemStack(gd.getStaticId("minecraft:item", "minecraft:bucket"), 1);
		return {Result::Success, createFilledResult(level, player, stack, std::move(empty))};
	}

	// BottleItem.use: filled with water from a water source (dragon's breath needs area effect clouds)
	Outcome useBottle(Level& level, Player& player, const ItemStack& stack) {
		const GameData&	   gd	 = level.gameData();
		ItemUse::HitResult hit	 = ItemUse::playerPOVHitResult(level, player, true);
		if (!hit.hit || !level.fluids().isWater(level.getFluidState(hit.pos).type)) return {Result::Pass, {}};
		level.playSoundAt(&player, player.getX(), player.getY(), player.getZ(), "minecraft:item.bottle.fill", Level::SoundSource::Neutral);
		ItemStack potion(gd.getStaticId("minecraft:item", "minecraft:potion"), 1);
		Components::set(potion, gd, "minecraft:potion_contents", Components::encodePotion(gd.getStaticId("minecraft:potion", "minecraft:water")));
		return {Result::Success, createFilledResult(level, player, stack, std::move(potion))};
	}

	// Item.use and the overrides of the items that don't need other entities
	Outcome use(Level& level, Player& player, int hand, const ItemStack& stack) {
		const GameData&			  gd	= level.gameData();
		const GD::ItemProperties* props = gd.getItemProperties(stack.item);
		if (!props) return {Result::Pass, {}};
		const std::string& name = itemName(gd, stack);
		if (name == "minecraft:bucket") return useBucket(level, player, stack, 0);
		if (name == "minecraft:water_bucket") return useBucket(level, player, stack, level.fluids().water());
		if (name == "minecraft:lava_bucket") return useBucket(level, player, stack, level.fluids().lava());
		if (name == "minecraft:glass_bottle") return useBottle(level, player, stack);
		if (name == "minecraft:spyglass") {
			playerSound(level, player, "minecraft:item.spyglass.use", 1.0F, 1.0F);
			ItemUse::startUsingItem(player, hand, gd);
			return {Result::Consume, {}};
		}
		// Hook for the items whose use makes an entity (projectiles, boats, spawn eggs...): nothing until they exist
		if (props->consumable) return startConsuming(level, player, hand, stack, *props);
		if (props->swappable) return swapWithEquipmentSlot(level, player, stack, *props);
		if (props->blocksAttacks) {
			ItemUse::startUsingItem(player, hand, gd);
			return {Result::Consume, {}};
		}
		return {Result::Pass, {}};
	}

	// Item.finishUsingItem through ItemStack.finishUsingItem
	ItemStack finishUsingItem(Level& level, Player& player, ItemStack stack) {
		const GameData&			  gd	 = level.gameData();
		const GD::ItemProperties* props	 = gd.getItemProperties(stack.item);
		ItemStack				  before = stack;
		if (itemName(gd, stack) == "minecraft:spyglass") {
			playerSound(level, player, "minecraft:item.spyglass.stop_using", 1.0F, 1.0F);
		} else if (props && props->consumable) {
			stack = onConsume(level, player, std::move(stack), *props);
		}
		return applyAfterUse(level, player, std::move(stack), before);
	}

	// ServerPlayer.completeUsingItem + LivingEntity.completeUsingItem
	void completeUsingItem(Level& level, Player& player) {
		SurvivalState& state = player.survival();
		if (state.useItem.isEmpty() || !player.isUsingItem()) return;
		Buffer event;
		event.writeInt(player.getPlayerID());
		event.writeByte(ENTITY_EVENT_USE_ITEM_COMPLETE);
		Packet::send(player.shared_from_this(), PacketId::Play::Clientbound::ENTITY_EVENT, event, level.server());
		int hand = player.getUsedItemHand();
		// useItem is the stack in the hand (updatingUsingItem just took it)
		const ItemStack& inHand = player.getStackInHand(hand);
		if (!state.useItem.sameItemSameComponents(inHand) || state.useItem.count != inHand.count) {
			ItemUse::releaseUsingItem(level, player);
			return;
		}
		ItemStack result = finishUsingItem(level, player, state.useItem);
		setItemInHand(player, hand, std::move(result));
		ItemUse::stopUsingItem(player);
	}

	// ----- Ray casts (BlockGetter.clip, VoxelShape.clip, AABB.clip) -----

	constexpr int DIRECTION_NORMALS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

	// Direction.getApproximateNearest (floats)
	Direction approximateNearest(double dx, double dy, double dz) {
		float	  x = static_cast<float>(dx), y = static_cast<float>(dy), z = static_cast<float>(dz);
		Direction best = Direction::North;
		float	  max  = std::numeric_limits<float>::denorm_min(); // Float.MIN_VALUE
		for (int d = 0; d < 6; d++) {
			float dot = x * DIRECTION_NORMALS[d][0] + y * DIRECTION_NORMALS[d][1] + z * DIRECTION_NORMALS[d][2];
			if (dot > max) {
				max	 = dot;
				best = static_cast<Direction>(d);
			}
		}
		return best;
	}

	// AABB.clipPoint
	bool clipPoint(double& t, double delta, double deltaB, double deltaC, double plane, double minB, double maxB, double minC, double maxC,
				   double start, double startB, double startC) {
		double s = (plane - start) / delta;
		double b = startB + s * deltaB;
		double c = startC + s * deltaC;
		if (0.0 < s && s < t && minB - 1.0E-7 < b && b < maxB + 1.0E-7 && minC - 1.0E-7 < c && c < maxC + 1.0E-7) {
			t = s;
			return true;
		}
		return false;
	}

	// AABB.getDirection: updates t and the face when this box is hit closer
	void clipBox(const AABB& box, const Vec3& from, double& t, std::optional<Direction>& face, double dx, double dy, double dz) {
		if (dx > 1.0E-7) {
			if (clipPoint(t, dx, dy, dz, box.minX, box.minY, box.maxY, box.minZ, box.maxZ, from.x, from.y, from.z)) face = Direction::West;
		} else if (dx < -1.0E-7) {
			if (clipPoint(t, dx, dy, dz, box.maxX, box.minY, box.maxY, box.minZ, box.maxZ, from.x, from.y, from.z)) face = Direction::East;
		}
		if (dy > 1.0E-7) {
			if (clipPoint(t, dy, dz, dx, box.minY, box.minZ, box.maxZ, box.minX, box.maxX, from.y, from.z, from.x)) face = Direction::Down;
		} else if (dy < -1.0E-7) {
			if (clipPoint(t, dy, dz, dx, box.maxY, box.minZ, box.maxZ, box.minX, box.maxX, from.y, from.z, from.x)) face = Direction::Up;
		}
		if (dz > 1.0E-7) {
			if (clipPoint(t, dz, dx, dy, box.minZ, box.minX, box.maxX, box.minY, box.maxY, from.z, from.x, from.y)) face = Direction::North;
		} else if (dz < -1.0E-7) {
			if (clipPoint(t, dz, dx, dy, box.maxZ, box.minX, box.maxX, box.minY, box.maxY, from.z, from.x, from.y)) face = Direction::South;
		}
	}

	// VoxelShape.clip: a ray starting inside the shape hits it right away, else the closest box face
	std::optional<ItemUse::HitResult> clipShape(const std::vector<GD::Box>& boxes, const Vec3& from, const Vec3& to, const BlockPos& pos) {
		if (boxes.empty()) return std::nullopt;
		Vec3 delta = to - from;
		if (delta.lengthSqr() < 1.0E-7) return std::nullopt;
		Vec3 inside = from + delta.scale(0.001);
		for (const GD::Box& b : boxes) {
			double x = inside.x - pos.x, y = inside.y - pos.y, z = inside.z - pos.z;
			if (x >= b.minX && x < b.maxX && y >= b.minY && y < b.maxY && z >= b.minZ && z < b.maxZ) {
				return ItemUse::HitResult{true, pos, Directions::opposite(approximateNearest(delta.x, delta.y, delta.z)), inside};
			}
		}
		double					 t = 1.0;
		std::optional<Direction> face;
		for (const GD::Box& b : boxes) {
			clipBox({pos.x + b.minX, pos.y + b.minY, pos.z + b.minZ, pos.x + b.maxX, pos.y + b.maxY, pos.z + b.maxZ}, from, t, face, delta.x, delta.y, delta.z);
		}
		if (!face) return std::nullopt;
		return ItemUse::HitResult{true, pos, *face, from + delta.scale(t)};
	}

	double frac(double value) { return value - std::floor(value); }
	double lerp(double delta, double a, double b) { return a + delta * (b - a); }
	int	   sign(double value) { return value == 0.0 ? 0 : value > 0.0 ? 1 : -1; }
} // namespace

namespace ItemUse {

	int enchantmentLevel(const GameData& gameData, const ItemStack& stack, const std::string& enchantment) {
		if (stack.isEmpty() || stack.components.empty()) return 0;
		std::optional<std::vector<uint8_t>> value = Components::get(stack, gameData, "minecraft:enchantments");
		if (!value) return 0;
		// ItemEnchantments.STREAM_CODEC: VarInt count, then (VarInt enchantment registry id, VarInt level) pairs
		const std::vector<uint8_t>& data = *value;
		size_t						pos	 = 0;
		auto						varint = [&](int& out) {
			   uint32_t result = 0;
			   for (int shift = 0; shift < 35; shift += 7) {
				   if (pos >= data.size()) return false;
				   uint8_t byte = data[pos++];
				   result |= static_cast<uint32_t>(byte & 0x7F) << shift;
				   if (!(byte & 0x80)) {
					   out = static_cast<int>(result);
					   return true;
				   }
			   }
			   return false;
		};
		int wanted = gameData.getSyncedId("minecraft:enchantment", enchantment);
		int count  = 0;
		if (wanted < 0 || !varint(count)) return 0;
		for (int i = 0; i < count; i++) {
			int id, level;
			if (!varint(id) || !varint(level)) return 0;
			if (id == wanted) return level;
		}
		return 0;
	}

	int getUseDuration(const GameData& gameData, const ItemStack& stack) {
		if (stack.isEmpty()) return 0;
		const GD::ItemProperties* props = gameData.getItemProperties(stack.item);
		if (!props) return 0;
		const std::string& name = itemName(gameData, stack);
		if (name == "minecraft:spyglass") return SPYGLASS_USE_DURATION;
		if (name == "minecraft:bow" || name == "minecraft:crossbow" || name == "minecraft:trident") return BLOCKING_USE_DURATION;
		if (props->consumable) return props->consumable->consumeTicks();
		return props->blocksAttacks ? BLOCKING_USE_DURATION : 0;
	}

	Result useItem(Level& level, Player& player, int hand) {
		if (player.getGameMode() == GameMode::Spectator) return Result::Pass;
		const GameData& gd	   = level.gameData();
		ItemStack		stack  = player.getStackInHand(hand);
		if (stack.isEmpty()) return Result::Pass;
		// ItemStack.use: the use_remainder of items used at once
		ItemStack before	  = stack;
		bool	  instant	  = getUseDuration(gd, stack) <= 0;
		Outcome	  outcome	  = use(level, player, hand, stack);
		bool	  success	  = outcome.result == Result::Success || outcome.result == Result::Consume;
		if (instant && success) outcome.transformed = applyAfterUse(level, player, outcome.transformed ? *outcome.transformed : player.getStackInHand(hand), before);

		// ServerPlayerGameMode.useItem
		ItemStack result = outcome.transformed ? *outcome.transformed : player.getStackInHand(hand);
		if (!outcome.transformed && result.count == before.count && getUseDuration(gd, result) <= 0) return outcome.result;
		if (outcome.result == Result::Fail && getUseDuration(gd, result) > 0 && !player.isUsingItem()) return outcome.result;
		if (outcome.transformed) setItemInHand(player, hand, result);
		if (!player.isUsingItem()) Menus::inventory(player, level).sendAllDataToRemote();
		return outcome.result;
	}

	void startUsingItem(Player& player, int hand, const GameData& gameData) {
		const ItemStack& stack = player.getStackInHand(hand);
		SurvivalState&	 state = player.survival();
		if (stack.isEmpty() || player.isUsingItem()) return;
		state.useItem		   = stack;
		state.useItemRemaining = getUseDuration(gameData, stack);
		state.livingFlags	   = static_cast<uint8_t>((state.livingFlags & ~2) | 1 | (hand == 1 ? 2 : 0));
		state.dirtyData |= Survival::DATA_LIVING_FLAGS;
	}

	void stopUsingItem(Player& player) {
		SurvivalState& state = player.survival();
		if (state.livingFlags & 1) {
			state.livingFlags &= ~1;
			state.dirtyData |= Survival::DATA_LIVING_FLAGS;
		}
		state.useItem		   = ItemStack();
		state.useItemRemaining = 0;
	}

	void updatingUsingItem(Level& level, Player& player) {
		if (!player.isUsingItem()) return;
		SurvivalState&	 state	= player.survival();
		const ItemStack& inHand = player.getStackInHand(player.getUsedItemHand());
		if (inHand.isEmpty() || state.useItem.isEmpty() || inHand.item != state.useItem.item) { // ItemStack.isSameItem
			stopUsingItem(player);
			return;
		}
		state.useItem = inHand;
		// LivingEntity.updateUsingItem: ItemStack.onUseTick, then completed when the time is over (crossbows wait for the release)
		const GD::ItemProperties* props = level.gameData().getItemProperties(inHand.item);
		if (props && props->consumable) {
			// Consumable.shouldEmitParticlesAndSounds: after the first 21.875 %, every 4 ticks
			int ticks = props->consumable->consumeTicks();
			if (ticks - state.useItemRemaining > static_cast<int>(ticks * 0.21875F) && state.useItemRemaining % 4 == 0) {
				emitParticlesAndSounds(level, player, *props->consumable, 5);
			}
		}
		if (--state.useItemRemaining == 0 && itemName(level.gameData(), inHand) != "minecraft:crossbow") completeUsingItem(level, player);
	}

	void releaseUsingItem(Level& level, Player& player) {
		SurvivalState&	 state	= player.survival();
		int				 hand	= player.getUsedItemHand();
		const ItemStack& inHand = player.getStackInHand(hand);
		if (!state.useItem.isEmpty() && !inHand.isEmpty() && inHand.item == state.useItem.item) {
			state.useItem = inHand;
			// Item.releaseUsing: the spyglass (bows, tridents... would shoot here)
			if (itemName(level.gameData(), inHand) == "minecraft:spyglass") {
				ItemStack before = inHand;
				playerSound(level, player, "minecraft:item.spyglass.stop_using", 1.0F, 1.0F);
				setItemInHand(player, hand, applyAfterUse(level, player, before, before));
			}
		}
		stopUsingItem(player);
	}

	HitResult clip(Level& level, const Vec3& fromPoint, const Vec3& toPoint, bool sourceFluids) {
		const GameData& gd = level.gameData();
		auto			at = [&](const BlockPos& pos) -> std::optional<HitResult> {
			   int					  state	   = level.getBlockState(pos);
			   std::optional<HitResult> blockHit = clipShape(gd.getOutlineShape(state), fromPoint, toPoint, pos);
			   std::optional<HitResult> fluidHit;
			   FluidState				fluid = level.fluids().stateOf(state);
			   if (sourceFluids && !fluid.isEmpty() && level.fluids().isSource(fluid)) {
				   // FlowingFluid.getShape: up to the fluid's height
				   fluidHit = clipShape({{0, 0, 0, 1, level.fluids().height(fluid, pos), 1}}, fromPoint, toPoint, pos);
			   }
			   double blockDistance = blockHit ? (blockHit->location - fromPoint).lengthSqr() : std::numeric_limits<double>::max();
			   double fluidDistance = fluidHit ? (fluidHit->location - fromPoint).lengthSqr() : std::numeric_limits<double>::max();
			   return blockDistance <= fluidDistance ? blockHit : fluidHit;
		};
		auto miss = [&]() {
			Vec3	  back = fromPoint - toPoint;
			HitResult result;
			result.face		= approximateNearest(back.x, back.y, back.z);
			result.pos		= {Mth::floor(toPoint.x), Mth::floor(toPoint.y), Mth::floor(toPoint.z)};
			result.location = toPoint;
			return result;
		};
		// BlockGetter.traverseBlocks
		if (fromPoint.x == toPoint.x && fromPoint.y == toPoint.y && fromPoint.z == toPoint.z) return miss();
		double toX = lerp(-1.0E-7, toPoint.x, fromPoint.x), toY = lerp(-1.0E-7, toPoint.y, fromPoint.y), toZ = lerp(-1.0E-7, toPoint.z, fromPoint.z);
		double fromX = lerp(-1.0E-7, fromPoint.x, toPoint.x), fromY = lerp(-1.0E-7, fromPoint.y, toPoint.y), fromZ = lerp(-1.0E-7, fromPoint.z, toPoint.z);
		int	   x = Mth::floor(fromX), y = Mth::floor(fromY), z = Mth::floor(fromZ);
		if (auto hit = at({x, y, z})) return *hit;
		double dx = toX - fromX, dy = toY - fromY, dz = toZ - fromZ;
		int	   sx = sign(dx), sy = sign(dy), sz = sign(dz);
		double stepX = sx == 0 ? std::numeric_limits<double>::max() : sx / dx;
		double stepY = sy == 0 ? std::numeric_limits<double>::max() : sy / dy;
		double stepZ = sz == 0 ? std::numeric_limits<double>::max() : sz / dz;
		double tx	 = stepX * (sx > 0 ? 1.0 - frac(fromX) : frac(fromX));
		double ty	 = stepY * (sy > 0 ? 1.0 - frac(fromY) : frac(fromY));
		double tz	 = stepZ * (sz > 0 ? 1.0 - frac(fromZ) : frac(fromZ));
		while (tx <= 1.0 || ty <= 1.0 || tz <= 1.0) {
			if (tx < ty) {
				if (tx < tz) {
					x += sx;
					tx += stepX;
				} else {
					z += sz;
					tz += stepZ;
				}
			} else if (ty < tz) {
				y += sy;
				ty += stepY;
			} else {
				z += sz;
				tz += stepZ;
			}
			if (auto hit = at({x, y, z})) return *hit;
		}
		return miss();
	}

	HitResult playerPOVHitResult(Level& level, const Player& player, bool sourceFluids) {
		Vec3 eye{player.getX(), player.getY() + Survival::eyeHeight(player), player.getZ()};
		// Entity.calculateViewVector
		float pitch = player.getPitch() * (static_cast<float>(M_PI) / 180.0F);
		float yaw	= -player.getYaw() * (static_cast<float>(M_PI) / 180.0F);
		float cosPitch = Mth::cos(pitch);
		Vec3  view{Mth::sin(yaw) * cosPitch, -Mth::sin(pitch), Mth::cos(yaw) * cosPitch};
		// Attributes.BLOCK_INTERACTION_RANGE: 4.5, + 0.5 in creative
		double range = player.getGameMode() == GameMode::Creative ? 5.0 : 4.5;
		return clip(level, eye, eye + view.scale(range), sourceFluids);
	}

} // namespace ItemUse
