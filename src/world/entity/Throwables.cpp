#include "world/entity/Throwables.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/ExperienceOrb.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/entity/MobEffects.hpp"
#include "world/entity/mobs/Animal.hpp"
#include "world/item/Components.hpp"

#include <cmath>

namespace {
	constexpr int SERIALIZER_ITEM_STACK = 7;
	constexpr int EVENT_BREAK			= 3; // The item particles of a broken snowball or egg
	constexpr int LEVEL_EVENT_POTION_SPLASH = 2002, LEVEL_EVENT_INSTANT_POTION_SPLASH = 2007;
	constexpr int DEFAULT_POTION_COLOR		= -13083194;

	int typeOf(Level& level, const char* type) { return level.gameData().getStaticId("minecraft:entity_type", type); }

	ItemStack orDefault(Level& level, ItemStack item, const char* fallback) {
		if (!item.isEmpty()) return item.copyWithCount(1);
		return ItemStack(level.gameData().getStaticId("minecraft:item", fallback), 1);
	}

	bool hurtActor(Level& level, Actor& target, const Combat::DamageSource& source, float amount) {
		if (Player* player = target.asPlayer()) return Combat::damage(level.server(), *player, amount, source);
		return target.hurtServer(source, amount);
	}

	// AABB.distanceToSqr(AABB)
	double boxDistanceSqr(const AABB& a, const AABB& b) {
		double dx = std::max(std::max(a.minX - b.maxX, b.minX - a.maxX), 0.0);
		double dy = std::max(std::max(a.minY - b.maxY, b.minY - a.maxY), 0.0);
		double dz = std::max(std::max(a.minZ - b.maxZ, b.minZ - a.maxZ), 0.0);
		return dx * dx + dy * dy + dz * dz;
	}
} // namespace

// ===================== ThrowableProjectile =====================

ThrowableProjectile::ThrowableProjectile(Level& level, const char* type, Actor* thrower, ItemStack item)
	: Projectile(level, typeOf(level, type), 0.25F, 0.25F), _item(std::move(item)) {
	if (thrower) {
		_position	 = {thrower->position().x, thrower->eyeY() - 0.1F, thrower->position().z};
		_oldPosition = _position;
		setOwner(thrower);
	}
}

void ThrowableProjectile::shootFromRotation(float xRot, float yRot, float offset, float velocity, float inaccuracy) {
	const float toRadians = static_cast<float>(M_PI / 180.0);
	float		x		  = -Mth::sin(yRot * toRadians) * Mth::cos(xRot * toRadians);
	float		y		  = -Mth::sin((xRot + offset) * toRadians);
	float		z		  = Mth::cos(yRot * toRadians) * Mth::cos(xRot * toRadians);
	shoot(x, y, z, velocity, inaccuracy); // The thrower's own movement isn't known for players: not added
}

void ThrowableProjectile::tick() {
	_delta.y -= gravity();								// applyGravity
	_delta = _delta.scale(isInWater() ? 0.8F : 0.99F);	// applyInertia
	// ProjectileUtil.getHitResultOnMoveVector: the first block, then the first entity before it
	Vec3			to	  = _position + _delta;
	Clip::HitResult block = Clip::clip(_level, _position, to, Clip::BlockMode::Collider, Clip::FluidMode::None);
	if (block.hit) to = block.location;
	std::optional<EntityHit> entity = findHitEntity(_position, to, boundingBox().expandTowards(_delta).inflate(1.0), computeMargin());
	bool					 miss	= !entity && !block.hit;
	_position						= entity ? entity->location : block.hit ? block.location : _position + _delta;
	updateRotation();
	_level.checkInsideBlocks(boundingBox(), this); // applyEffectsFromBlocks
	Projectile::tick();
	if (!miss && isAlive()) {
		Hit hit;
		hit.location = _position;
		if (entity) {
			hit.actor = entity->actor;
		} else {
			hit.pos	 = block.pos;
			hit.face = block.face;
		}
		onHit(hit);
	}
}

void ThrowableProjectile::writeEntityData(Buffer& buf) const {
	buf.writeUByte(DATA_ITEM_STACK);
	buf.writeVarInt(SERIALIZER_ITEM_STACK);
	_item.write(buf);
}

void ThrowableProjectile::breakAndDiscard() {
	Buffer event;
	event.writeInt(_id);
	event.writeByte(EVENT_BREAK);
	_level.entities().broadcast(*this, PacketId::Play::Clientbound::ENTITY_EVENT, event);
	discard();
}

// ===================== Snowball =====================

Snowball::Snowball(Level& level, Actor* thrower, ItemStack item)
	: ThrowableProjectile(level, "minecraft:snowball", thrower, orDefault(level, std::move(item), "minecraft:snowball")) {}

void Snowball::onHit(const Hit& hit) {
	if (hit.actor) {
		bool blaze = _level.gameData().getStaticName("minecraft:entity_type", hit.actor->typeId()) == "minecraft:blaze";
		hurtActor(_level, *hit.actor, {"minecraft:thrown", owner(), this, std::nullopt}, blaze ? 3.0F : 0.0F);
	}
	breakAndDiscard();
}

// ===================== ThrownEgg =====================

ThrownEgg::ThrownEgg(Level& level, Actor* thrower, ItemStack item)
	: ThrowableProjectile(level, "minecraft:egg", thrower, orDefault(level, std::move(item), "minecraft:egg")) {}

void ThrownEgg::onHit(const Hit& hit) {
	if (hit.actor) hurtActor(_level, *hit.actor, {"minecraft:thrown", owner(), this, std::nullopt}, 0.0F);
	if (_random.nextInt(8) == 0) {
		int count = _random.nextInt(32) == 0 ? 4 : 1;
		for (int i = 0; i < count; i++) {
			std::unique_ptr<Mob> chick = _level.mobs().create(_level, typeOf(_level, "minecraft:chicken"));
			if (!chick) break;
			if (auto* ageable = dynamic_cast<AgeableMob*>(chick.get())) ageable->setAge(AgeableMob::BABY_START_AGE);
			chick->snapTo(_position, _yRot, 0.0F);
			_level.addFreshEntity(std::move(chick)); // The egg's chicken variant isn't kept
		}
	}
	breakAndDiscard();
}

// ===================== ThrownExperienceBottle =====================

ThrownExperienceBottle::ThrownExperienceBottle(Level& level, Actor* thrower, ItemStack item)
	: ThrowableProjectile(level, "minecraft:experience_bottle", thrower, orDefault(level, std::move(item), "minecraft:experience_bottle")) {}

void ThrownExperienceBottle::onHit(const Hit& hit) {
	_level.levelEvent(nullptr, LEVEL_EVENT_POTION_SPLASH, blockPosition(), DEFAULT_POTION_COLOR);
	JavaRandom& random = _level.random();
	int			value  = 3 + random.nextInt(5) + random.nextInt(5);
	// ExperienceOrb.awardWithDirection (its direction isn't given to the orb here)
	if (auto orb = ExperienceOrb::create(_level, hit.location, value)) _level.entities().add(std::move(orb));
	discard();
}

// ===================== ThrownSplashPotion =====================

ThrownSplashPotion::ThrownSplashPotion(Level& level, Actor* thrower, ItemStack item)
	: ThrowableProjectile(level, "minecraft:splash_potion", thrower, orDefault(level, std::move(item), "minecraft:splash_potion")) {}

void ThrownSplashPotion::dowseFire(const BlockPos& pos) {
	int block = _level.blocks().blockOf(_level.getBlockState(pos));
	if (_level.gameData().isInTag("minecraft:block", "minecraft:fire", block)) _level.destroyBlock(pos, false);
	// Candles and campfires aren't put out yet
}

void ThrownSplashPotion::onHit(const Hit& hit) {
	const GameData& data   = _level.gameData();
	int				potion = -1;
	if (std::optional<std::vector<uint8_t>> contents = Components::get(_item, data, "minecraft:potion_contents")) potion = Components::decodePotion(*contents);
	std::string name = potion >= 0 ? data.getStaticName("minecraft:potion", potion) : "";
	bool		water = name == "minecraft:water";
	if (!hit.actor && water) {
		// AbstractThrownPotion.onHitBlock: the fire around where it broke
		BlockPos at = hit.pos.relative(hit.face);
		dowseFire(at);
		dowseFire(at.relative(Directions::opposite(hit.face)));
		for (Direction direction : Directions::HORIZONTAL) dowseFire(at.relative(direction));
	}
	std::vector<MobEffects::Instance> effects = MobEffects::potionEffects(name);
	if (water) {
		onHitAsWater();
	} else if (!effects.empty()) {
		onHitAsPotion(name, 1.0F, hit.location);
	}
	// The splash, in the potion's color (instant potions have their own)
	bool instant = false;
	int	 r = 0, g = 0, b = 0, total = 0;
	for (const MobEffects::Instance& effect : effects) {
		instant |= MobEffects::info(effect.effect).instantaneous;
		int c	= MobEffects::info(effect.effect).color;
		int amp = effect.amplifier + 1;
		r += amp * (c >> 16 & 0xFF);
		g += amp * (c >> 8 & 0xFF);
		b += amp * (c & 0xFF);
		total += amp;
	}
	int color = total == 0 ? DEFAULT_POTION_COLOR
						   : static_cast<int>(0xFF000000u | static_cast<uint32_t>(r / total) << 16 | static_cast<uint32_t>(g / total) << 8 | static_cast<uint32_t>(b / total));
	_level.levelEvent(nullptr, instant ? LEVEL_EVENT_INSTANT_POTION_SPLASH : LEVEL_EVENT_POTION_SPLASH, blockPosition(), color);
	discard();
}

void ThrownSplashPotion::onHitAsWater() {
	// What water hurts (endermen, blazes, snow golems, striders) takes 1 damage; what burns is put out
	AABB area = boundingBox().inflate(4.0, 2.0, 4.0);
	_level.entities().forEachIn(area, [&](Entity& entity) {
		LivingEntity* living = entity.asLiving();
		if (!living || living->isRemoved() || distanceToSqr(living->position()) >= 16.0) return;
		if (living->isSensitiveToWater()) living->hurtServer({"minecraft:indirect_magic", owner(), this, std::nullopt}, 1.0F);
		if (living->isOnFire() && living->isAlive()) living->setRemainingFireTicks(0);
	});
	for (const auto& player : _level.players()) {
		if (player->boundingBox().intersects(area) && distanceToSqr(player->position()) < 16.0 && player->survival().remainingFireTicks > 0) {
			player->survival().remainingFireTicks = 0;
		}
	}
}

void ThrownSplashPotion::onHitAsPotion(const std::string& potion, float durationScale, const Vec3& location) {
	std::vector<MobEffects::Instance> effects = MobEffects::potionEffects(potion);
	AABB							  box	  = boundingBox().move(location - _position);
	AABB							  area	  = box.inflate(4.0, 2.0, 4.0);
	float							  margin  = computeMargin();
	Actor*							  source  = effectSource();
	auto							  splash  = [&](Actor& target, MobEffects::Host& host, MobEffects::Active& active) {
		   double distance = boxDistanceSqr(box, target.boundingBox().inflate(margin));
		   if (distance >= 16.0) return;
		   double strength = 1.0 - std::sqrt(distance) / 4.0;
		   for (const MobEffects::Instance& effect : effects) {
			   if (MobEffects::info(effect.effect).instantaneous) {
				   MobEffects::applyInstantaneous(host, effect, strength, this, owner());
			   } else {
				   MobEffects::Instance scaled = effect;
				   if (!scaled.isInfiniteDuration() && scaled.duration != 0) scaled.duration = static_cast<int>(strength * scaled.duration * durationScale + 0.5);
				   scaled.showIcon = scaled.visible;
				   if (!scaled.endsWithin(20)) active.add(host, scaled, source != nullptr);
			   }
		   }
	};
	// LivingEntity.isAffectedByPotions: not the dying, nor spectators
	for (const auto& player : _level.players()) {
		if (player->isSpectator() || player->combat().dead || !player->boundingBox().intersects(area)) continue;
		splash(*player, *player, player->effects());
	}
	std::vector<LivingEntity*> living;
	_level.entities().forEachIn(area, [&](Entity& entity) {
		if (LivingEntity* l = entity.asLiving(); l && !l->isRemoved() && l->isAlive()) living.push_back(l);
	});
	for (LivingEntity* target : living) splash(*target, *target, target->effects());
}
