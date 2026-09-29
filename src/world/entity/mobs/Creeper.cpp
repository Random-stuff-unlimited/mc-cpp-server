#include "world/entity/mobs/Creeper.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Explosion.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"
#include "world/item/ItemDamage.hpp"

namespace {
	constexpr int SAVE_VERSION = 1;
}

void Creeper::setSwellDir(int dir) {
	if (_swellDir == dir) return;
	_swellDir = dir;
	markData(DATA_SWELL_DIR);
}

void Creeper::setPowered(bool powered) {
	if (_powered == powered) return;
	_powered = powered;
	markData(DATA_IS_POWERED);
}

void Creeper::ignite() {
	if (_ignited) return;
	_ignited = true;
	markData(DATA_IS_IGNITED);
}

void Creeper::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<SwellGoal>(*this));
	_goalSelector.addGoal(3, std::make_unique<AvoidEntityGoal>(*this, "Ocelot", 6.0F, 1.0, 1.2));
	_goalSelector.addGoal(3, std::make_unique<AvoidEntityGoal>(*this, "Cat", 6.0F, 1.0, 1.2));
	_goalSelector.addGoal(4, std::make_unique<MeleeAttackGoal>(*this, 1.0, false));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 0.8));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(6, std::make_unique<RandomLookAroundGoal>(*this));
	_targetSelector.addGoal(1, std::make_unique<NearestAttackableTargetGoal>(*this, "Player", true));
	_targetSelector.addGoal(2, std::make_unique<HurtByTargetGoal>(*this));
}

void Creeper::tick() {
	if (isAlive()) {
		if (_ignited) setSwellDir(1);
		// The PRIME_FUSE game event (swell starting) isn't listened to by anything ported
		_swell += _swellDir;
		if (_swell < 0) _swell = 0;
		if (_swell >= _maxSwell) {
			_swell = _maxSwell;
			explodeCreeper();
			if (isRemoved()) return;
		}
	}
	Monster::tick();
}

// Creeper.getMaxFallDistance: getComfortableFallDistance(0 or health - 1)
int Creeper::getMaxFallDistance() { return getTarget() ? Mth::floor(_health - 1.0F + 3.0F) : 3; }

void Creeper::setTarget(Actor* target) {
	if (target && isOfClass(*target, "Goat")) return;
	Monster::setTarget(target);
}

bool Creeper::causeFallDamage(double distance, float multiplier) {
	bool hurt = Monster::causeFallDamage(distance, multiplier);
	_swell += static_cast<int>(distance * 1.5);
	if (_swell > _maxSwell - 5) _swell = _maxSwell - 5;
	return hurt;
}

bool Creeper::mobInteract(Player& player, int hand) {
	ItemStack& stack = player.inventory().getMutable(player.handSlot(hand));
	if (stack.isEmpty() || !_level.gameData().isInTag("minecraft:item", "minecraft:creeper_igniters", stack.item)) return Monster::mobInteract(player, hand);
	const std::string& name	 = _level.gameData().getStaticName("minecraft:item", stack.item);
	const char*		   sound = name == "minecraft:fire_charge" ? "minecraft:item.firecharge.use" : "minecraft:item.flintandsteel.use";
	_level.playSoundAt(&player, _position.x, _position.y, _position.z, sound, Level::SoundSource::Hostile, 1.0F, _random.nextFloat() * 0.4F + 0.8F);
	ignite();
	if (ItemDamage::maxDamage(_level.gameData(), stack) <= 0) {
		stack.shrink(1);
	} else {
		ItemDamage::hurtAndBreak(_level, stack, 1, &player, player.handSlot(hand));
	}
	return true;
}

void Creeper::explodeCreeper() {
	float multiplier = _powered ? 2.0F : 1.0F;
	_dead			 = true;
	Explosions::explode(_level, this, std::nullopt, nullptr, _position, _explosionRadius * multiplier, false, Explosions::Interaction::Mob);
	// spawnLingeringCloud: mob effects aren't ported yet, so it has none to leave
	discard();
}

void Creeper::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Monster::writeData(buf, mask, onlyNonDefault);
	if (wantsData(mask, DATA_SWELL_DIR, onlyNonDefault, _swellDir == -1)) writeIntData(buf, DATA_SWELL_DIR, _swellDir);
	if (wantsData(mask, DATA_IS_POWERED, onlyNonDefault, !_powered)) writeBoolData(buf, DATA_IS_POWERED, _powered);
	if (wantsData(mask, DATA_IS_IGNITED, onlyNonDefault, !_ignited)) writeBoolData(buf, DATA_IS_IGNITED, _ignited);
}

void Creeper::save(Buffer& buf) const {
	Monster::save(buf);
	buf.writeUByte(SAVE_VERSION);
	buf.writeBool(_powered);
	buf.writeShort(static_cast<int16_t>(_maxSwell));
	buf.writeByte(static_cast<int8_t>(_explosionRadius));
	buf.writeBool(_ignited);
}

void Creeper::load(Buffer& buf) {
	Monster::load(buf);
	if (buf.readUByte() != SAVE_VERSION) throw std::runtime_error("unknown creeper data version");
	_powered		 = buf.readBool();
	_maxSwell		 = buf.readShort();
	_explosionRadius = buf.readByte();
	if (buf.readBool()) _ignited = true;
	_dirtyData = 0;
}

// ----- SwellGoal -----

SwellGoal::SwellGoal(Creeper& creeper) : _creeper(creeper) { setFlags({Flag::Move}); }

bool SwellGoal::canUse() {
	Actor* target = _creeper.getTarget();
	return _creeper.getSwellDir() > 0 || (target && _creeper.distanceToSqr(*target) < 9.0);
}

void SwellGoal::start() {
	_creeper.navigation().stop();
	Actor* target = _creeper.getTarget();
	if (target) {
		_target = EntityRef(*target);
	} else {
		_target.clear();
	}
}

void SwellGoal::tick() {
	Actor* target = _target.isSet() ? _creeper.level().actorByRef(_target) : nullptr;
	if (!target || _creeper.distanceToSqr(*target) > 49.0 || !_creeper.sensing().hasLineOfSight(*target)) {
		_creeper.setSwellDir(-1);
	} else {
		_creeper.setSwellDir(1);
	}
}
