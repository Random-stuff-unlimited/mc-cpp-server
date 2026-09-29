#include "world/Explosion.hpp"

#include "PacketIds.hpp"
#include "lib/JavaHashSet.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "network/server.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Level.hpp"
#include "world/blocks/Fire.hpp"
#include "world/entity/ItemEntity.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/entity/PrimedTnt.hpp"
#include "world/item/ItemDamage.hpp"

#include <algorithm>
#include <cmath>

namespace {
	const Explosions::Calculator DEFAULT_CALCULATOR;
	constexpr int				 MAX_DROPS_PER_COMBINED_STACK = 16;

	// ServerLevel.explode: the game rules' defaults (mobGriefing on, block and mob explosions decay, TNT's don't)
	Explosions::BlockInteraction blockInteraction(Explosions::Interaction interaction) {
		switch (interaction) {
		case Explosions::Interaction::None: return Explosions::BlockInteraction::Keep;
		case Explosions::Interaction::Block:
		case Explosions::Interaction::Mob: return Explosions::BlockInteraction::DestroyWithDecay;
		case Explosions::Interaction::Tnt: return Explosions::BlockInteraction::Destroy;
		case Explosions::Interaction::Trigger: return Explosions::BlockInteraction::TriggerBlock;
		}
		return Explosions::BlockInteraction::Keep;
	}

	void writeParticle(Buffer& buf, const GameData& data, const char* particle) { buf.writeVarInt(data.getStaticId("minecraft:particle_type", particle)); }

	// ServerExplosion.calculateExplodedPositions: rays from the 16x16x16 grid's surface, their power spent by the blocks
	std::vector<BlockPos> explodedPositions(Explosions::Explosion& e) {
		JavaHashSet<BlockPos> positions;
		Level&				  level = e.level;
		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				for (int k = 0; k < 16; k++) {
					if (!(i == 0 || i == 15 || j == 0 || j == 15 || k == 0 || k == 15)) continue;
					double dx = i / 15.0F * 2.0F - 1.0F, dy = j / 15.0F * 2.0F - 1.0F, dz = k / 15.0F * 2.0F - 1.0F;
					double length = std::sqrt(dx * dx + dy * dy + dz * dz);
					dx /= length;
					dy /= length;
					dz /= length;
					float  power = e.radius * (0.7F + level.random().nextFloat() * 0.6F);
					double x = e.center.x, y = e.center.y, z = e.center.z;
					for (; power > 0.0F; power -= 0.22500001F) {
						BlockPos pos{Mth::floor(x), Mth::floor(y), Mth::floor(z)};
						if (level.isOutsideBuildHeight(pos.y)) break; // isInWorldBounds
						int		   state = level.getBlockState(pos);
						FluidState fluid = level.fluids().stateOf(state);
						if (std::optional<float> resistance = e.calculator.blockExplosionResistance(level, pos, state, fluid)) {
							power -= (*resistance + 0.3F) * 0.3F;
						}
						if (power > 0.0F && e.calculator.shouldBlockExplode(level, pos, state, power)) positions.add(pos);
						x += dx * 0.3F;
						y += dy * 0.3F;
						z += dz * 0.3F;
					}
				}
			}
		}
		return positions.values();
	}

	// ExplosionDamageCalculator.getEntityDamageAmount
	float entityDamage(const Explosions::Explosion& e, Actor& actor, float seen) {
		float  diameter = e.radius * 2.0F;
		double distance = std::sqrt(actor.distanceToSqr(e.center)) / diameter;
		double impact	= (1.0 - distance) * seen;
		return static_cast<float>((impact * impact + impact) / 2.0 * 7.0 * diameter + 1.0);
	}

	// ServerExplosion.hurtEntities: the players' knockback, for their explosion packet
	std::vector<std::pair<Player*, Vec3>> hurtEntities(Explosions::Explosion& e) {
		std::vector<std::pair<Player*, Vec3>> hitPlayers;
		float								  diameter = e.radius * 2.0F;
		AABB box{static_cast<double>(Mth::floor(e.center.x - diameter - 1.0)), static_cast<double>(Mth::floor(e.center.y - diameter - 1.0)),
				 static_cast<double>(Mth::floor(e.center.z - diameter - 1.0)), static_cast<double>(Mth::floor(e.center.x + diameter + 1.0)),
				 static_cast<double>(Mth::floor(e.center.y + diameter + 1.0)), static_cast<double>(Mth::floor(e.center.z + diameter + 1.0))};
		for (Actor* actor : e.level.actorsIn(box, e.source)) {
			if (!actor->isAlive() && !actor->asEntity()) continue;
			if (actor->isSpectator()) continue; // Entity.ignoreExplosion
			double distance = std::sqrt(actor->distanceToSqr(e.center)) / diameter;
			if (distance > 1.0) continue;
			bool		 isTnt	   = dynamic_cast<PrimedTnt*>(actor) != nullptr;
			Vec3		 from	   = isTnt ? actor->position() : actor->eyePosition();
			Vec3		 direction = (from - e.center).normalize();
			bool		 damages   = e.calculator.shouldDamageEntity(*actor);
			float		 knockback = e.calculator.knockbackMultiplier(*actor);
			float		 seen	   = !damages && knockback == 0.0F ? 0.0F : Explosions::seenPercent(e.level, e.center, actor->boundingBox());
			if (damages) actor->hurtServer(e.damage, entityDamage(e, *actor, seen));
			double resistance = 0.0;
			if (LivingEntity* living = actor->asLiving()) resistance = living->getAttributeValue(living->attributeIds().explosionKnockbackResistance);
			double strength = (1.0 - distance) * seen * knockback * (1.0 - resistance);
			Vec3   push		= direction.scale(strength);
			if (Player* player = actor->asPlayer()) {
				// Its client moves it: the knockback goes in its explosion packet
				if (!player->isSpectator() && !(player->isCreative() && player->survival().flying)) hitPlayers.emplace_back(player, push);
			} else {
				actor->pushMotion(push);
			}
		}
		return hitPlayers;
	}

	// ItemEntity.areMergable and merge, with at most 16 per stack
	void addOrAppendStack(std::vector<std::pair<ItemStack, BlockPos>>& stacks, ItemStack stack, const BlockPos& pos, const GameData& data) {
		for (auto& [existing, at] : stacks) {
			const GameData::ItemProperties* props = data.getItemProperties(existing.item);
			int								max	  = std::min(props ? props->maxStackSize : 64, MAX_DROPS_PER_COMBINED_STACK);
			if (!existing.sameItemSameComponents(stack) || existing.count + stack.count > max) {
				// areMergable also needs both below their size: merge what fits
				if (!existing.sameItemSameComponents(stack) || existing.count >= max || stack.count >= max) continue;
			}
			int moved = std::min(max - existing.count, stack.count);
			existing.grow(moved);
			stack.shrink(moved);
			if (stack.isEmpty()) return;
		}
		stacks.emplace_back(std::move(stack), pos);
	}

	void interactWithBlocks(Explosions::Explosion& e, std::vector<BlockPos>& positions) {
		Level& level = e.level;
		// Util.shuffle
		for (size_t i = positions.size(); i > 1; i--) {
			size_t j = static_cast<size_t>(level.random().nextInt(static_cast<int>(i)));
			std::swap(positions[i - 1], positions[j]);
		}
		std::vector<std::pair<ItemStack, BlockPos>> stacks;
		const GameData&								data = level.gameData();
		for (const BlockPos& pos : positions) {
			int state = level.getBlockState(pos);
			level.behavior(state).onExplosionHit(level, pos, state, e,
												 [&](ItemStack stack, const BlockPos& at) { addOrAppendStack(stacks, std::move(stack), at, data); });
		}
		for (auto& [stack, pos] : stacks) level.popResource(pos, std::move(stack));
	}

	void createFire(Explosions::Explosion& e, const std::vector<BlockPos>& positions) {
		Level& level = e.level;
		for (const BlockPos& pos : positions) {
			if (level.random().nextInt(3) == 0 && level.blocks().isAir(level.getBlockState(pos)) &&
				level.gameData().getStateProperties(level.getBlockState(pos.below())).solidRender) {
				level.setBlock(pos, BaseFireBlock::getState(level, pos), Level::UPDATE_ALL);
			}
		}
	}
} // namespace

namespace Explosions {

	std::optional<float> Calculator::blockExplosionResistance(Level& level, const BlockPos&, int state, const FluidState& fluid) const {
		if (level.blocks().isAir(state) && fluid.isEmpty()) return std::nullopt;
		float blockResistance = level.gameData().getBlockProperties(state).explosionResistance;
		// FluidState.getExplosionResistance: water and lava are 100
		float fluidResistance = fluid.isEmpty() ? 0.0F : 100.0F;
		return std::max(blockResistance, fluidResistance);
	}

	Actor* Explosion::indirectSource() const {
		if (!source) return nullptr;
		if (auto* tnt = dynamic_cast<PrimedTnt*>(source)) return tnt->owner();
		if (source->asLiving() || source->isPlayer()) return source;
		if (Entity* entity = source->asEntity()) return entity->owner();
		return nullptr;
	}

	float seenPercent(Level& level, const Vec3& center, const AABB& box) {
		double stepX = 1.0 / ((box.maxX - box.minX) * 2.0 + 1.0);
		double stepY = 1.0 / ((box.maxY - box.minY) * 2.0 + 1.0);
		double stepZ = 1.0 / ((box.maxZ - box.minZ) * 2.0 + 1.0);
		double offsetX = (1.0 - std::floor(1.0 / stepX) * stepX) / 2.0;
		double offsetZ = (1.0 - std::floor(1.0 / stepZ) * stepZ) / 2.0;
		if (stepX < 0.0 || stepY < 0.0 || stepZ < 0.0) return 0.0F;
		int seen = 0, total = 0;
		for (double x = 0.0; x <= 1.0; x += stepX) {
			for (double y = 0.0; y <= 1.0; y += stepY) {
				for (double z = 0.0; z <= 1.0; z += stepZ) {
					Vec3 point{box.minX + x * (box.maxX - box.minX) + offsetX, box.minY + y * (box.maxY - box.minY),
							   box.minZ + z * (box.maxZ - box.minZ) + offsetZ};
					if (!Clip::clip(level, point, center, Clip::BlockMode::Collider, Clip::FluidMode::None).hit) seen++;
					total++;
				}
			}
		}
		return static_cast<float>(seen) / total;
	}

	void explode(Level& level, Actor* source, std::optional<Combat::DamageSource> damage, const Calculator* calculator, const Vec3& center, float radius,
				 bool fire, Interaction interaction) {
		Explosion e{level, source, {}, calculator ? *calculator : DEFAULT_CALCULATOR, center, radius, fire, blockInteraction(interaction)};
		if (damage) {
			e.damage = *damage;
		} else {
			// DamageSources.explosion(direct, indirect): player_explosion when someone is responsible
			Actor* indirect = e.indirectSource();
			e.damage		= {indirect && source ? "minecraft:player_explosion" : "minecraft:explosion", indirect, source, std::nullopt};
		}
		std::vector<BlockPos>				  positions	 = explodedPositions(e);
		std::vector<std::pair<Player*, Vec3>> hitPlayers = hurtEntities(e);
		if (e.interaction != BlockInteraction::Keep) interactWithBlocks(e, positions);
		if (e.fire) createFire(e, positions);

		// ServerLevel.explode: the packet to each player within 64 blocks
		bool			small = e.radius < 2.0F || e.interaction == BlockInteraction::Keep;
		const GameData& data  = level.gameData();
		for (const auto& player : level.players()) {
			if (player->isDisconnected() || player->distanceToSqr(center) >= 4096.0) continue;
			Buffer packet;
			packet.writeDouble(center.x);
			packet.writeDouble(center.y);
			packet.writeDouble(center.z);
			packet.writeFloat(radius);
			packet.writeInt(static_cast<int32_t>(positions.size()));
			auto hit = std::find_if(hitPlayers.begin(), hitPlayers.end(), [&](const auto& h) { return h.first == player.get(); });
			packet.writeBool(hit != hitPlayers.end());
			if (hit != hitPlayers.end()) {
				packet.writeDouble(hit->second.x);
				packet.writeDouble(hit->second.y);
				packet.writeDouble(hit->second.z);
			}
			writeParticle(packet, data, small ? "minecraft:explosion" : "minecraft:explosion_emitter");
			packet.writeVarInt(data.getStaticId("minecraft:sound_event", "minecraft:entity.generic.explode") + 1);
			// Level.DEFAULT_EXPLOSION_BLOCK_PARTICLES: poof (scaling 0.5) and smoke, weight 1 each
			packet.writeVarInt(2);
			writeParticle(packet, data, "minecraft:poof");
			packet.writeFloat(0.5F);
			packet.writeFloat(1.0F);
			packet.writeVarInt(1);
			writeParticle(packet, data, "minecraft:smoke");
			packet.writeFloat(1.0F);
			packet.writeFloat(1.0F);
			packet.writeVarInt(1);
			Packet::send(player, PacketId::Play::Clientbound::EXPLODE, packet, level.server());
		}
	}

	bool primeTnt(Level& level, const BlockPos& pos, Actor* owner) {
		auto tnt = std::make_unique<PrimedTnt>(level, Vec3{pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}, owner);
		Vec3 at	 = tnt->position();
		level.addFreshEntity(std::move(tnt));
		level.playSoundAt(nullptr, at.x, at.y, at.z, "minecraft:entity.tnt.primed", Level::SoundSource::Blocks, 1.0F, 1.0F);
		return true;
	}

} // namespace Explosions

// ===================== TntBlock =====================

TntBlock::TntBlock(std::shared_ptr<const BlockContext> context) : _context(std::move(context)), _unstable(_context->blocks.property("unstable")) {}

void TntBlock::onPlace(Level& level, const BlockPos& pos, int state, int oldState, bool) const {
	if (_context->blocks.blockOf(oldState) == _context->blocks.blockOf(state)) return;
	if (level.hasNeighborSignal(pos) && Explosions::primeTnt(level, pos, nullptr)) level.removeBlock(pos, false);
}

void TntBlock::neighborChanged(Level& level, const BlockPos& pos, int, int, bool) const {
	if (level.hasNeighborSignal(pos) && Explosions::primeTnt(level, pos, nullptr)) level.removeBlock(pos, false);
}

void TntBlock::playerWillDestroy(Level& level, const BlockPos& pos, int state, Player& player) const {
	if (!player.isCreative() && _context->blocks.getBool(state, _unstable)) Explosions::primeTnt(level, pos, nullptr);
}

UseResult TntBlock::useItemOn(Level& level, const BlockPos& pos, int, Player& player, int hand, const BlockHit&) const {
	ItemStack&		   stack = player.inventory().getMutable(player.handSlot(hand));
	const std::string& name	 = level.gameData().getStaticName("minecraft:item", stack.item);
	if (name != "minecraft:flint_and_steel" && name != "minecraft:fire_charge") return UseResult::TryWithEmptyHand;
	if (Explosions::primeTnt(level, pos, &player)) {
		level.setBlock(pos, level.gameData().getDefaultBlockState("minecraft:air"), Level::UPDATE_ALL_IMMEDIATE);
		if (!player.isCreative()) {
			if (name == "minecraft:flint_and_steel") {
				ItemDamage::hurtAndBreak(level, stack, 1, &player, player.handSlot(hand));
			} else {
				stack.shrink(1);
			}
		}
	}
	return UseResult::Success;
}

void TntBlock::wasExploded(Level& level, const BlockPos& pos, Explosions::Explosion& explosion) const {
	auto tnt  = std::make_unique<PrimedTnt>(level, Vec3{pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}, explosion.indirectSource());
	int	 fuse = tnt->fuse();
	tnt->setFuse(level.random().nextInt(fuse / 4) + fuse / 8);
	level.addFreshEntity(std::move(tnt));
}
