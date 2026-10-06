#include "world/entity/Arrow.hpp"

#include "PacketIds.hpp"
#include "network/buffer.hpp"
#include "network/packet.hpp"
#include "world/entity/ItemEntity.hpp"
#include "player.hpp"
#include "world/Clip.hpp"
#include "world/Combat.hpp"
#include "world/Level.hpp"
#include "world/entity/LivingEntity.hpp"
#include "world/item/Enchantments.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	constexpr int	 SAVE_VERSION		  = 1;
	constexpr int	 SERIALIZER_BYTE = 0, SERIALIZER_INT = 1, SERIALIZER_BOOLEAN = 8;
	constexpr int	 SHAKE_TIME			  = 7;
	constexpr int	 DESPAWN_TIME		  = 1200;
	constexpr int	 EXPOSED_POTION_DECAY_TIME = 600;
	constexpr float	 WATER_INERTIA = 0.6F, INERTIA = 0.99F;
	constexpr double GRAVITY	   = 0.05;
	constexpr int	 EVENT_POTION_PUFF = 0;
	constexpr int	 GAME_EVENT_ARROW_HIT_PLAYER = 6; // ClientboundGameEventPacket.PLAY_ARROW_HIT_SOUND

	// RandomSource.triangle
	double triangle(JavaRandom& random, double mode, double deviation) { return mode + deviation * (random.nextDouble() - random.nextDouble()); }

	int typeOf(Level& level, const char* type) { return level.gameData().getStaticId("minecraft:entity_type", type); }
} // namespace

Arrow::Arrow(Level& level, const char* type, const char* defaultItem) : Projectile(level, typeOf(level, type), 0.5F, 0.5F) {
	_pickupItem = ItemStack(level.gameData().getStaticId("minecraft:item", defaultItem), 1);
	if (std::string(type) == "minecraft:trident") _soundEvent = "minecraft:item.trident.hit_ground";
}

Arrow::Arrow(Level& level, LivingEntity& shooter, ItemStack pickupItem, const ItemStack* weapon, const char* type, const char* defaultItem)
	: Arrow(level, type, defaultItem) {
	if (!pickupItem.isEmpty()) _pickupItem = pickupItem.copyWithCount(1);
	_position	 = {shooter.position().x, shooter.eyeY() - 0.1F, shooter.position().z};
	_oldPosition = _position;
	if (weapon && !weapon->isEmpty()) {
		_firedFromWeapon = *weapon;
		_hasWeapon		 = true;
		// EnchantmentHelper.getPiercingCount (crossbows only, not ported here)
	}
	setOwner(&shooter);
}

void Arrow::setOwner(Actor* owner) {
	Projectile::setOwner(owner);
	// A player's arrow can be picked up
	if (owner && owner->asPlayer() && _pickup == Pickup::Disallowed) _pickup = Pickup::Allowed;
}

void Arrow::shoot(double x, double y, double z, float velocity, float inaccuracy) {
	Projectile::shoot(x, y, z, velocity, inaccuracy);
	_life = 0;
}

void Arrow::setBaseDamageFromMob(float power) { _baseDamage = power * 2.0F + triangle(_random, _level.difficulty() * 0.11, 0.57425); }

void Arrow::setFlag(int flag, bool value) {
	uint8_t flags = static_cast<uint8_t>(value ? _flags | flag : _flags & ~flag);
	if (flags == _flags) return;
	_flags			= flags;
	entityDataDirty = true;
}

void Arrow::setInGround(bool inGround) {
	if (inGround == _inGround) return;
	_inGround		= inGround;
	entityDataDirty = true;
}

void Arrow::addEffect(const MobEffects::Instance& effect) {
	_effects.push_back(effect);
	updateEffectColor();
}

void Arrow::updateEffectColor() {
	// PotionContents.getColor: the visible effects' colors weighted by their levels
	int color = -1;
	if (!_effects.empty()) {
		int r = 0, g = 0, b = 0, total = 0;
		for (const MobEffects::Instance& effect : _effects) {
			if (!effect.visible) continue;
			int c	= MobEffects::info(effect.effect).color;
			int amp = effect.amplifier + 1;
			r += amp * (c >> 16 & 0xFF);
			g += amp * (c >> 8 & 0xFF);
			b += amp * (c & 0xFF);
			total += amp;
		}
		color = total == 0 ? -13083194 : static_cast<int>(0xFF000000u | static_cast<uint32_t>(r / total) << 16 | static_cast<uint32_t>(g / total) << 8 |
															static_cast<uint32_t>(b / total));
	}
	if (color == _effectColor) return;
	_effectColor	= color;
	entityDataDirty = true;
}

// ----- Tick -----

bool Arrow::isInWaterOrRain() {
	BlockPos top{Mth::floor(_position.x), Mth::floor(boundingBox().maxY), Mth::floor(_position.z)};
	return isInWater() || _level.isRainingAt(blockPosition()) || _level.isRainingAt(top);
}

bool Arrow::shouldFall() {
	// Nothing solid left around where it stuck
	return _inGround && !_level.hasBlockCollision(AABB{_position.x, _position.y, _position.z, _position.x, _position.y, _position.z}.inflate(0.06));
}

void Arrow::startFalling() {
	setInGround(false);
	_delta = _delta.multiply(_random.nextFloat() * 0.2F, _random.nextFloat() * 0.2F, _random.nextFloat() * 0.2F);
	_life  = 0;
}

void Arrow::applyFireEffectsFromBlocks() {
	// The fire, lava and water inside its box: fire and lava light it, water puts it out (Entity.applyEffectsFromBlocks)
	_level.checkInsideBlocks(boundingBox(), this);
	AABB	box	 = boundingBox().deflate(1.0E-5);
	bool	fire = false, lava = false, water = false;
	Fluids& fluids = _level.fluids();
	for (int x = Mth::floor(box.minX); x <= Mth::floor(box.maxX); x++) {
		for (int y = Mth::floor(box.minY); y <= Mth::floor(box.maxY); y++) {
			for (int z = Mth::floor(box.minZ); z <= Mth::floor(box.maxZ); z++) {
				BlockPos pos{x, y, z};
				int		 state = _level.getBlockState(pos);
				int		 block = _level.blocks().blockOf(state);
				fire |= block == _level.fireBlock() || block == _level.soulFireBlock();
				FluidState fluid = fluids.stateOf(state);
				if (fluid.type == 0 || y + fluids.height(fluid, pos) < box.minY) continue;
				lava |= fluids.isLava(fluid.type);
				water |= fluids.isWater(fluid.type);
			}
		}
	}
	if (fire) {
		// BaseFireBlock.fireIgnite
		if (_remainingFireTicks < 0) _remainingFireTicks++;
		if (_remainingFireTicks >= 0) igniteForTicks(160);
	}
	if (lava) igniteForTicks(300);
	if (water) clearFire();
}

void Arrow::tick() {
	bool physics = !_noPhysics;
	Vec3 delta	 = _delta;
	// Inside a block's collision shape: stuck
	BlockPos pos   = blockPosition();
	int		 state = _level.getBlockState(pos);
	if (physics && !_level.blocks().isAir(state)) {
		for (const GameData::Box& b : _level.gameData().getCollisionShape(state)) {
			AABB box{pos.x + b.minX, pos.y + b.minY, pos.z + b.minZ, pos.x + b.maxX, pos.y + b.maxY, pos.z + b.maxZ};
			if (box.contains(_position)) {
				_delta = {0.0, 0.0, 0.0};
				setInGround(true);
				break;
			}
		}
	}
	if (_shakeTime > 0) _shakeTime--;
	if (isInWaterOrRain()) clearFire();

	if (_inGround && physics) {
		if (_lastState != state && shouldFall()) {
			startFalling();
		} else if (++_life >= DESPAWN_TIME) { // tickDespawn
			discard();
		}
		_inGroundTime++;
		if (isAlive()) applyFireEffectsFromBlocks();
		// setSharedFlagOnFire: seen burning (entity data 0)
		entityDataDirty = true;
	} else {
		_inGroundTime = 0;
		Vec3 start	  = _position;
		if (isInWater()) _delta = _delta.scale(waterInertia()); // The bubbles are the clients'
		float yaw	= static_cast<float>(Mth::atan2(physics ? delta.x : -delta.x, physics ? delta.z : -delta.z) * 180.0F / (float)M_PI);
		float pitch = static_cast<float>(Mth::atan2(delta.y, std::sqrt(delta.horizontalDistanceSqr())) * 180.0F / (float)M_PI);
		_xRot		= lerpRotation(_xRot, pitch);
		_yRot		= lerpRotation(_yRot, yaw);
		checkLeftOwner();
		if (physics) {
			Clip::HitResult block = Clip::clip(_level, start, start + delta, Clip::BlockMode::Collider, Clip::FluidMode::None);
			stepMoveAndHit(block);
		} else {
			_position = start + delta;
			applyFireEffectsFromBlocks();
		}
		if (!isInWater()) _delta = _delta.scale(INERTIA);
		if (physics && !_inGround) _delta.y -= GRAVITY; // applyGravity
		if (!isRemoved()) Projectile::tick();
	}

	// Arrow.tick: a tipped arrow stuck for 30 seconds loses its effects
	if (!isRemoved() && _inGround && _inGroundTime != 0 && !_effects.empty() && _inGroundTime >= EXPOSED_POTION_DECAY_TIME) {
		Buffer event;
		event.writeInt(_id);
		event.writeByte(EVENT_POTION_PUFF);
		_level.entities().broadcast(*this, PacketId::Play::Clientbound::ENTITY_EVENT, event);
		_effects.clear();
		_pickupItem = ItemStack(_level.gameData().getStaticId("minecraft:item", "minecraft:arrow"), 1);
		updateEffectColor();
	}
}

void Arrow::stepMoveAndHit(const Clip::HitResult& block) {
	while (isAlive()) {
		Vec3					 from = _position;
		std::optional<EntityHit> hit  = findHitEntity(from, block.location, boundingBox().expandTowards(_delta).inflate(1.0), computeMargin());
		Vec3					 to	  = hit ? hit->location : block.location;
		_position					  = to;
		_level.checkInsideBlocks(boundingBox(), this); // applyEffectsFromBlocks along the way
		if (!hit) {
			if (isAlive() && block.hit) {
				onHitBlock(block.pos, block.location);
				hasImpulse = true;
			}
			break;
		}
		if (isAlive() && !_noPhysics) {
			onHitEntity(*hit->actor);
			hasImpulse = true;
			if (_pierceLevel > 0) continue;
		}
		break;
	}
}

bool Arrow::canHitEntity(Actor& target) {
	if (!Projectile::canHitEntity(target)) return false;
	return std::find(_piercingIgnore.begin(), _piercingIgnore.end(), target.id()) == _piercingIgnore.end();
}

void Arrow::onHitEntity(Actor& target) {
	float  speed  = static_cast<float>(_delta.length());
	double damage = _baseDamage;
	Actor* owner  = this->owner();
	Combat::DamageSource source{"minecraft:arrow", owner ? owner : this, this, std::nullopt};
	if (_hasWeapon) damage = Enchantments::modifyDamage(_level, _firedFromWeapon, target, source, static_cast<float>(damage));
	int amount = static_cast<int>(std::ceil(std::clamp(speed * damage, 0.0, 2.147483647E9)));
	if (_pierceLevel > 0) {
		if (static_cast<int>(_piercingIgnore.size()) >= _pierceLevel + 1) {
			discard();
			return;
		}
		_piercingIgnore.push_back(target.id());
	}
	if (isCritArrow()) {
		int64_t bonus = _random.nextInt(amount / 2 + 2);
		amount		  = static_cast<int>(std::min<int64_t>(bonus + amount, std::numeric_limits<int32_t>::max()));
	}
	if (owner) {
		if (LivingEntity* living = owner->asLiving()) living->setLastHurtMob(&target);
	}
	bool enderman = _level.gameData().getStaticName("minecraft:entity_type", target.typeId()) == "minecraft:enderman";
	int	 fireBefore = 0;
	if (Entity* entity = target.asEntity()) fireBefore = entity->remainingFireTicks();
	if (isOnFire() && !enderman) target.igniteForTicks(100); // igniteForSeconds(5)

	bool hurt = false;
	if (Player* player = target.asPlayer()) {
		hurt = Combat::damage(_level.server(), *player, static_cast<float>(amount), source);
	} else if (LivingEntity* living = target.asLiving()) {
		hurt = living->hurtServer(source, static_cast<float>(amount));
	}
	if (hurt) {
		if (enderman) return;
		// The arrow count shown stuck in the target, the knockback of Punch, the effects
		if (_hasWeapon) {
			float knockback = Enchantments::modifyKnockback(_level, _firedFromWeapon, target, source, 0.0F);
			if (knockback > 0.0F) {
				double resistance = 0.0;
				if (LivingEntity* living = target.asLiving()) resistance = living->getAttributeValue(living->attributeIds().knockbackResistance);
				Vec3 push = Vec3{_delta.x, 0.0, _delta.z}.normalize().scale(knockback * 0.6 * std::max(0.0, 1.0 - resistance));
				if (push.lengthSqr() > 0.0) target.pushMotion({push.x, 0.1, push.z});
			}
			Enchantments::doPostAttackEffects(_level, _firedFromWeapon, owner ? *owner : static_cast<Actor&>(*this), target, source);
		}
		for (const MobEffects::Instance& effect : _effects) LivingEntity::addEffectTo(target, effect, effectSource()); // doPostHurtEffects
		if (target.asPlayer() && owner && owner->asPlayer() && owner != &target) {
			Buffer event;
			event.writeUByte(GAME_EVENT_ARROW_HIT_PLAYER);
			event.writeFloat(0.0F);
			Packet::send(owner->asPlayer()->shared_from_this(), PacketId::Play::Clientbound::GAME_EVENT, event, _level.server());
		}
		_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, _soundEvent, Level::SoundSource::Neutral, 1.0F,
						   1.2F / (_random.nextFloat() * 0.2F + 0.9F));
		if (_pierceLevel <= 0) discard();
	} else {
		// Not hurt (a shield, invulnerable): it bounces back, slowed down
		if (Entity* entity = target.asEntity()) entity->setRemainingFireTicks(fireBefore);
		deflectReverse();
		_delta = _delta.scale(0.2);
		if (_delta.lengthSqr() < 1.0E-7) {
			if (_pickup == Pickup::Allowed) {
				_level.entities().add(ItemEntity::create(_level, {_position.x, _position.y + 0.1F, _position.z}, _pickupItem));
			}
			discard();
		}
	}
}

void Arrow::deflectReverse() {
	float turn = 170.0F + _random.nextFloat() * 20.0F;
	_delta	   = _delta.scale(-0.5);
	_yRot += turn;
	_yRotO += turn;
	hasImpulse = true;
}

void Arrow::onHitBlock(const BlockPos& pos, const Vec3& location) {
	_lastState = _level.getBlockState(pos);
	// BlockState.onProjectileHit (targets, bells, tnt...) isn't ported
	(void)location;
	Vec3 back{_delta.x > 0 ? 1.0 : _delta.x < 0 ? -1.0 : 0.0, _delta.y > 0 ? 1.0 : _delta.y < 0 ? -1.0 : 0.0,
			  _delta.z > 0 ? 1.0 : _delta.z < 0 ? -1.0 : 0.0};
	_position = _position - back.scale(0.05F);
	_delta	  = {0.0, 0.0, 0.0};
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, _soundEvent, Level::SoundSource::Neutral, 1.0F,
					   1.2F / (_random.nextFloat() * 0.2F + 0.9F));
	setInGround(true);
	_shakeTime = SHAKE_TIME;
	setCritArrow(false);
	if (_pierceLevel != 0) {
		_pierceLevel	= 0;
		entityDataDirty = true;
	}
	_soundEvent = "minecraft:entity.arrow.hit";
	_piercingIgnore.clear();
}

int Arrow::touchedByPlayer(Player& player) {
	if (!(_inGround || _noPhysics) || _shakeTime > 0 || isRemoved()) return 0;
	bool taken = false;
	switch (_pickup) {
	case Pickup::Disallowed: break;
	case Pickup::Allowed: {
		ItemStack stack = _pickupItem;
		player.inventory().add(stack, player.getSelectedSlot(), player.isCreative(), _level.gameData());
		taken = stack.isEmpty();
		break;
	}
	case Pickup::CreativeOnly: taken = player.isCreative(); break;
	}
	if (!taken) return 0;
	discard();
	return 1;
}

// ----- Entity data and saving -----

void Arrow::writeEntityData(Buffer& buf) const {
	buf.writeUByte(0); // Entity.DATA_SHARED_FLAGS_ID: on fire
	buf.writeVarInt(SERIALIZER_BYTE);
	buf.writeUByte(_remainingFireTicks > 0 ? 1 : 0);
	buf.writeUByte(DATA_FLAGS);
	buf.writeVarInt(SERIALIZER_BYTE);
	buf.writeUByte(_flags);
	buf.writeUByte(DATA_PIERCE_LEVEL);
	buf.writeVarInt(SERIALIZER_BYTE);
	buf.writeUByte(_pierceLevel);
	buf.writeUByte(DATA_IN_GROUND);
	buf.writeVarInt(SERIALIZER_BOOLEAN);
	buf.writeBool(_inGround);
	if (_hasEffectColorData) {
		buf.writeUByte(DATA_EFFECT_COLOR);
		buf.writeVarInt(SERIALIZER_INT);
		buf.writeVarInt(_effectColor);
	}
}

void Arrow::save(Buffer& buf) const {
	buf.writeUByte(SAVE_VERSION);
	saveBase(buf);
	saveProjectile(buf);
	buf.writeShort(static_cast<int16_t>(_life));
	buf.writeVarInt(_lastState);
	buf.writeUByte(static_cast<uint8_t>(_shakeTime));
	buf.writeBool(_inGround);
	buf.writeUByte(static_cast<uint8_t>(_pickup));
	buf.writeDouble(_baseDamage);
	buf.writeBool(isCritArrow());
	buf.writeUByte(_pierceLevel);
	buf.writeString(_soundEvent);
	buf.writeVarInt(_pickupItem.item);
	buf.writeVarInt(static_cast<int32_t>(_effects.size()));
	for (const MobEffects::Instance& effect : _effects) {
		buf.writeVarInt(effect.effect);
		buf.writeVarInt(effect.duration);
		buf.writeVarInt(effect.amplifier);
		buf.writeUByte(effect.packetFlags(false));
	}
}

void Arrow::load(Buffer& buf) {
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown arrow data version");
	loadBase(buf);
	loadProjectile(buf);
	_life		= buf.readShort();
	_lastState	= buf.readVarInt();
	_shakeTime	= buf.readUByte();
	_inGround	= buf.readBool();
	int pickup	= buf.readUByte();
	_pickup		= pickup <= 2 ? static_cast<Pickup>(pickup) : Pickup::Disallowed;
	_baseDamage = buf.readDouble();
	_flags		= buf.readBool() ? FLAG_CRIT : 0;
	_pierceLevel = buf.readUByte();
	_soundEvent	 = buf.readString();
	_pickupItem	 = ItemStack(buf.readVarInt(), 1);
	_effects.clear();
	for (int i = buf.readVarInt(); i > 0; i--) {
		int		effect	  = buf.readVarInt();
		int		duration  = buf.readVarInt();
		int		amplifier = buf.readVarInt();
		uint8_t flags	  = buf.readUByte();
		if (effect >= 0 && effect < MobEffects::Count) {
			_effects.emplace_back(static_cast<MobEffects::Effect>(effect), duration, amplifier, flags & 1, flags & 2, flags & 4);
		}
	}
	_effectColor = -2;
	updateEffectColor();
}

// ===================== ThrownTrident =====================

ThrownTrident::ThrownTrident(Level& level) : Arrow(level, "minecraft:trident", "minecraft:trident") { _hasEffectColorData = false; }

ThrownTrident::ThrownTrident(Level& level, LivingEntity& shooter, ItemStack trident)
	: Arrow(level, shooter, std::move(trident), nullptr, "minecraft:trident", "minecraft:trident") {
	_hasEffectColorData = false;
}

void ThrownTrident::onHitEntity(Actor& target) {
	Actor*				 owner = this->owner();
	Combat::DamageSource source{"minecraft:trident", owner ? owner : this, this, std::nullopt};
	float				 damage = Enchantments::modifyDamage(_level, _pickupItem, target, source, 8.0F); // Its weapon is itself
	_dealtDamage				= true;
	bool hurt = target.asPlayer() ? Combat::damage(_level.server(), *target.asPlayer(), damage, source) : target.hurtServer(source, damage);
	if (hurt && _level.gameData().getStaticName("minecraft:entity_type", target.typeId()) == "minecraft:enderman") return;
	deflectReverse();
	_delta = _delta.multiply(0.02, 0.2, 0.02);
	_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:item.trident.hit", Level::SoundSource::Neutral, 1.0F, 1.0F);
}

void ThrownTrident::save(Buffer& buf) const {
	Arrow::save(buf);
	buf.writeBool(_dealtDamage);
}

void ThrownTrident::load(Buffer& buf) {
	Arrow::load(buf);
	_dealtDamage = buf.readBool();
}
