#include "world/entity/mobs/Wolf.hpp"

#include "network/buffer.hpp"
#include "player.hpp"
#include "world/Level.hpp"
#include "world/entity/ai/Goals.hpp"

namespace {
	// WolfEntity's entity data
	constexpr int DATA_COLLAR_COLOR_ID = 19;
	constexpr int EVENT_TAME_SUCCESS	= 20, EVENT_TAME_FAILED = 21; // EntityEvent
	// WolfEntity.tame: a bone has a 33 % chance to tame it
	constexpr float TAME_CHANCE = 1.0F / 3.0F;
} // namespace

Wolf::Wolf(Level& level, int typeId) : Mob(level, typeId) {}

Actor* Wolf::owner() const { return _owner.isSet() ? _level.actorByRef(_owner) : nullptr; }

bool Wolf::_isMeat(const ItemStack& stack) const {
	return !stack.isEmpty() && _level.gameData().isInTag("minecraft:item", "minecraft:meat", stack.item);
}

void Wolf::usePlayerItem(Player& player, int hand) {
	if (!player.isCreative()) player.inventory().getMutable(player.handSlot(hand)).shrink(1);
}

void Wolf::tame(Player& player) {
	_tamed = true;
	_owner = EntityRef(player);
	markData(DATA_COLLAR_COLOR_ID); // The collar appears (red)
	setTarget(nullptr);
	// Tamed wolves no longer hunt wild animals
	_targetSelector.removeAllGoals();
	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
}

bool Wolf::mobInteract(Player& player, int hand) {
	const ItemStack& stack = player.getStackInHand(hand);
	// Meat: heal a tamed wolf (4 health per piece)
	if (_isMeat(stack) && _tamed) {
		if (health() < 20.0F) {
			usePlayerItem(player, hand);
			heal(4.0F);
			_level.playSoundAt(nullptr, _position.x, _position.y, _position.z, "minecraft:entity.generic.eat", Level::SoundSource::Neutral, 1.0F,
							   1.0F);
			return true;
		}
		return false;
	}
	// Bones: tame it, a third of the time
	if (!_tamed && stack.item == _level.gameData().getStaticId("minecraft:item", "minecraft:bone")) {
		usePlayerItem(player, hand);
		if (_level.random().nextFloat() < TAME_CHANCE) {
			tame(player);
			broadcastEntityEvent(EVENT_TAME_SUCCESS);
		} else {
			broadcastEntityEvent(EVENT_TAME_FAILED);
		}
		return true;
	}
	return Mob::mobInteract(player, hand);
}

void Wolf::writeData(Buffer& buf, uint32_t mask, bool onlyNonDefault) const {
	Mob::writeData(buf, mask, onlyNonDefault);
	// The collar, only for a tamed wolf (the client renders it as tamed from there)
	if (wantsData(mask, DATA_COLLAR_COLOR_ID, onlyNonDefault, !_tamed)) writeIntData(buf, DATA_COLLAR_COLOR_ID, _collarColor);
}

void Wolf::save(Buffer& buf) const {
	Mob::save(buf);
	buf.writeBool(_tamed);
	buf.writeBool(_owner.isSet());
	if (_owner.isSet()) buf.writeUUID(_owner.uuid);
	buf.writeVarInt(_collarColor);
}

void Wolf::load(Buffer& buf) {
	Mob::load(buf);
	_tamed = buf.readBool();
	if (buf.readBool()) _owner.uuid = buf.readUUID();
	_collarColor = buf.readVarInt();
}

bool Wolf::wantsToPickUp(const ItemStack& stack) {
	return _isMeat(stack) ? false : Mob::wantsToPickUp(stack);
}

void Wolf::registerGoals() {
	_goalSelector.addGoal(1, std::make_unique<FloatGoal>(*this));
	_goalSelector.addGoal(2, std::make_unique<PanicGoal>(*this, 1.5));
	_goalSelector.addGoal(3, std::make_unique<MeleeAttackGoal>(*this, 1.0, true));
	_goalSelector.addGoal(4, std::make_unique<FollowOwnerGoal>(*this, 1.0, 10.0F, 2.0F));
	_goalSelector.addGoal(5, std::make_unique<WaterAvoidingRandomStrollGoal>(*this, 1.0));
	_goalSelector.addGoal(6, std::make_unique<LookAtPlayerGoal>(*this, "Player", 8.0F));
	_goalSelector.addGoal(7, std::make_unique<RandomLookAroundGoal>(*this));

	_targetSelector.addGoal(1, std::make_unique<HurtByTargetGoal>(*this));
	// Wild wolves hunt; tamed ones stop (the targets are re-registered when it is tamed)
	_targetSelector.addGoal(2, std::make_unique<NearestAttackableTargetGoal>(*this, "Sheep", false));
	_targetSelector.addGoal(3, std::make_unique<NearestAttackableTargetGoal>(*this, "Rabbit", false));
	_targetSelector.addGoal(4, std::make_unique<NearestAttackableTargetGoal>(*this, "Skeleton", false));
}